#!/usr/bin/env python3
"""
Tidal Music Service for HeadUnit.

Provides a JSON-based Unix socket server that the Qt6/C++ app connects to.
Handles Tidal authentication, search, browsing, and stream URL retrieval.

Protocol: newline-delimited JSON messages over a Unix domain socket.
Request:  {"cmd": "search", "query": "radiohead", "type": "tracks", "limit": 20}
Response: {"cmd": "search", "ok": true, "data": [...]}

Commands:
  auth_status    - Check if logged in
  auth_login     - Start OAuth device login flow
  search         - Search tracks/albums/artists/playlists
  get_track      - Get track details + stream URL
  get_album      - Get album tracks
  get_artist     - Get artist top tracks/albums
  get_playlist   - Get playlist tracks
  favorites      - Get user favorites
  play_track     - Get stream URL for playback
"""

import asyncio
import json
import os
import sys
import signal
import logging
from pathlib import Path

import tidalapi

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [TidalService] %(levelname)s: %(message)s',
    datefmt='%H:%M:%S'
)
log = logging.getLogger('tidal')

SOCKET_PATH = '/tmp/headunit_tidal.sock'
SESSION_FILE = os.path.expanduser('~/.config/headunit/tidal_session.json')
QUALITY = tidalapi.Quality.low_320k  # Default to 320kbps


class TidalService:
    def __init__(self):
        self.session = tidalapi.Session()
        self._login_uri = None
        self._auth_future = None

    def load_session(self):
        """Try to restore a previous session from disk."""
        if not os.path.exists(SESSION_FILE):
            return False

        try:
            with open(SESSION_FILE) as f:
                data = json.load(f)

            self.session.load_oauth_session(
                token_type=data['token_type'],
                access_token=data['access_token'],
                refresh_token=data.get('refresh_token'),
                expiry_time=data.get('expiry_time'),
            )

            if self.session.check_login():
                log.info("Session restored from disk")
                return True
            else:
                log.info("Saved session expired, need re-login")
                return False
        except Exception as e:
            log.warning(f"Failed to load session: {e}")
            return False

    def save_session(self):
        """Persist session tokens to disk."""
        os.makedirs(os.path.dirname(SESSION_FILE), exist_ok=True)
        data = {
            'token_type': self.session.token_type,
            'access_token': self.session.access_token,
            'refresh_token': self.session.refresh_token,
            'expiry_time': self.session.expiry_time.isoformat() if self.session.expiry_time else None,
        }
        with open(SESSION_FILE, 'w') as f:
            json.dump(data, f)
        log.info("Session saved to disk")

    def is_logged_in(self):
        return self.session.check_login()

    def start_device_login(self):
        """Start OAuth device authorization flow. Returns the login URI for the user."""
        login, future = self.session.login_oauth()
        self._login_uri = login.verification_uri_complete
        self._auth_future = future
        log.info(f"Device login started: {self._login_uri}")
        return {
            'verification_uri': login.verification_uri_complete,
            'user_code': login.user_code,
            'expires_in': login.expires_in,
        }

    def check_device_login(self):
        """Check if the user completed the device login."""
        if self._auth_future is None:
            return False

        if self._auth_future.running():
            return False

        try:
            self._auth_future.result(timeout=0)
            if self.session.check_login():
                self.save_session()
                log.info("Login successful!")
                self._auth_future = None
                return True
        except Exception as e:
            log.warning(f"Login check failed: {e}")

        return False

    def search(self, query, search_type='tracks', limit=20, offset=0):
        """Search Tidal catalog."""
        results = self.session.search(query, models=[self._type_model(search_type)], limit=limit, offset=offset)

        if search_type == 'tracks':
            return [self._track_to_dict(t) for t in results.get('tracks', results.get('top_hit', []))]
        elif search_type == 'albums':
            return [self._album_to_dict(a) for a in results.get('albums', [])]
        elif search_type == 'artists':
            return [self._artist_to_dict(a) for a in results.get('artists', [])]
        elif search_type == 'playlists':
            return [self._playlist_to_dict(p) for p in results.get('playlists', [])]
        return []

    def get_track_stream_url(self, track_id):
        """Get the actual stream URL for a track."""
        try:
            track = self.session.track(track_id)
            stream = track.get_stream()
            manifest = stream.get_stream_manifest()
            urls = manifest.get_urls()
            if urls:
                return {
                    'url': urls[0],
                    'codec': manifest.codecs or '',
                    'quality': stream.audio_quality or '',
                    'bit_depth': stream.bit_depth,
                    'sample_rate': stream.sample_rate,
                    'title': track.name or '',
                    'artist': track.artist.name if track.artist else '',
                    'album': track.album.name if track.album else '',
                    'image_url': track.album.image(640) if track.album else '',
                    'duration': track.duration or 0,
                }
        except Exception as e:
            log.error(f"Failed to get stream URL for track {track_id}: {e}")
        return None

    def get_album_tracks(self, album_id):
        """Get all tracks in an album."""
        album = self.session.album(album_id)
        tracks = album.tracks()
        return {
            'album': self._album_to_dict(album),
            'tracks': [self._track_to_dict(t) for t in tracks],
        }

    def get_artist_info(self, artist_id):
        """Get artist top tracks and albums."""
        artist = self.session.artist(artist_id)
        top_tracks = artist.get_top_tracks(limit=10)
        albums = artist.get_albums(limit=20)
        return {
            'artist': self._artist_to_dict(artist),
            'top_tracks': [self._track_to_dict(t) for t in top_tracks],
            'albums': [self._album_to_dict(a) for a in albums],
        }

    def get_playlist_tracks(self, playlist_id):
        """Get all tracks in a playlist."""
        playlist = self.session.playlist(playlist_id)
        tracks = playlist.tracks()
        return {
            'playlist': self._playlist_to_dict(playlist),
            'tracks': [self._track_to_dict(t) for t in tracks],
        }

    def get_favorites(self):
        """Get user's favorite tracks."""
        if not self.is_logged_in():
            return []
        favs = self.session.user.favorites
        tracks = favs.tracks(limit=50)
        return [self._track_to_dict(t) for t in tracks]

    def add_favorite(self, track_id):
        """Add a track to user favorites."""
        if not self.is_logged_in():
            return False
        try:
            self.session.user.favorites.add_track(track_id)
            log.info(f"Added track {track_id} to favorites")
            return True
        except Exception as e:
            log.error(f"Failed to add favorite: {e}")
            return False

    def remove_favorite(self, track_id):
        """Remove a track from user favorites."""
        if not self.is_logged_in():
            return False
        try:
            self.session.user.favorites.remove_track(track_id)
            log.info(f"Removed track {track_id} from favorites")
            return True
        except Exception as e:
            log.error(f"Failed to remove favorite: {e}")
            return False

    def get_home(self):
        """Get the full home page data: mixes, favorite albums, favorite artists, and home page sections."""
        if not self.is_logged_in():
            return {}

        result = {'sections': []}

        # 1. Mixes For You (My Mix 1-8, Daily Discovery, etc.)
        try:
            mixes = self.session.mixes()
            mix_list = []
            for m in mixes[:12]:
                d = self._mix_to_dict(m)
                # Try to get mix image
                try:
                    images = m.image(640)
                    d['image_url'] = images
                except Exception:
                    d['image_url'] = ''
                mix_list.append(d)
            if mix_list:
                result['sections'].append({
                    'title': 'Mixes For You',
                    'type': 'mixes',
                    'items': mix_list,
                })
        except Exception as e:
            log.warning(f"Failed to get mixes: {e}")

        # 2. Favorite Albums (recently added first)
        try:
            favs = self.session.user.favorites
            fav_albums = favs.albums(limit=20)
            albums = [self._album_to_dict(a) for a in fav_albums]
            if albums:
                result['sections'].append({
                    'title': 'Your Albums',
                    'type': 'albums',
                    'items': albums,
                })
        except Exception as e:
            log.warning(f"Failed to get favorite albums: {e}")

        # 3. Favorite Artists
        try:
            favs = self.session.user.favorites
            fav_artists = favs.artists(limit=20)
            artists = [self._artist_to_dict(a) for a in fav_artists]
            if artists:
                result['sections'].append({
                    'title': 'Your Artists',
                    'type': 'artists',
                    'items': artists,
                })
        except Exception as e:
            log.warning(f"Failed to get favorite artists: {e}")

        # 4. Favorite Tracks (for quick access row)
        try:
            favs = self.session.user.favorites
            fav_tracks = favs.tracks(limit=20)
            tracks = [self._track_to_dict(t) for t in fav_tracks]
            if tracks:
                result['sections'].append({
                    'title': 'Your Tracks',
                    'type': 'tracks',
                    'items': tracks,
                })
        except Exception as e:
            log.warning(f"Failed to get favorite tracks: {e}")

        # 5. Home page content from Tidal API (suggested albums, playlists, etc.)
        try:
            home = self.session.home()
            if home and hasattr(home, 'categories'):
                for cat in home.categories[:6]:
                    try:
                        title = getattr(cat, 'title', '') or ''
                        cat_type = str(getattr(cat, 'type', ''))
                        items = []

                        if not hasattr(cat, 'items') or not cat.items:
                            continue

                        for item in cat.items[:15]:
                            if isinstance(item, tidalapi.album.Album):
                                items.append(self._album_to_dict(item))
                            elif isinstance(item, tidalapi.artist.Artist):
                                items.append(self._artist_to_dict(item))
                            elif isinstance(item, tidalapi.media.Track):
                                items.append(self._track_to_dict(item))
                            elif isinstance(item, tidalapi.playlist.Playlist):
                                items.append(self._playlist_to_dict(item))
                            elif isinstance(item, tidalapi.mix.Mix) or isinstance(item, tidalapi.mix.MixV2):
                                d = self._mix_to_dict(item)
                                try:
                                    d['image_url'] = item.image(640)
                                except Exception:
                                    d['image_url'] = ''
                                items.append(d)

                        if items and title:
                            # Map category type to our item types
                            section_type = 'albums'  # default
                            if 'TRACK' in cat_type:
                                section_type = 'tracks'
                            elif 'ARTIST' in cat_type:
                                section_type = 'artists'
                            elif 'PLAYLIST' in cat_type:
                                section_type = 'playlists'
                            elif 'MIX' in cat_type:
                                section_type = 'mixes'
                            elif 'MIXED' in cat_type:
                                section_type = 'mixed'

                            result['sections'].append({
                                'title': title,
                                'type': section_type,
                                'items': items,
                            })
                    except Exception as e:
                        log.warning(f"Failed to parse home category: {e}")
        except Exception as e:
            log.warning(f"Failed to get home page: {e}")

        log.info(f"Home page: {len(result['sections'])} sections")
        return result

    def get_mix_tracks(self, mix_id):
        """Get tracks from a specific mix."""
        if not self.is_logged_in():
            return {}
        try:
            mix = self.session.mix(mix_id)
            tracks = mix.items()
            return {
                'mix': self._mix_to_dict(mix),
                'tracks': [self._track_to_dict(t) for t in tracks if isinstance(t, tidalapi.media.Track)],
            }
        except Exception as e:
            log.error(f"Failed to get mix {mix_id}: {e}")
            return {}

    # ── Helpers ──

    def _type_model(self, search_type):
        return {
            'tracks': tidalapi.media.Track,
            'albums': tidalapi.album.Album,
            'artists': tidalapi.artist.Artist,
            'playlists': tidalapi.playlist.Playlist,
        }.get(search_type, tidalapi.media.Track)

    def _track_to_dict(self, track):
        return {
            'id': track.id,
            'title': track.name,
            'version': getattr(track, 'version', '') or '',
            'artist': track.artist.name if track.artist else 'Unknown',
            'artist_id': track.artist.id if track.artist else None,
            'album': track.album.name if track.album else '',
            'album_id': track.album.id if track.album else None,
            'duration': track.duration,
            'track_num': track.track_num,
            'explicit': getattr(track, 'explicit', False),
            'audio_quality': getattr(track, 'audio_quality', ''),
            'image_url': track.album.image(640) if track.album else '',
            'image_url_small': track.album.image(160) if track.album else '',
        }

    def _album_to_dict(self, album):
        return {
            'id': album.id,
            'title': album.name,
            'artist': album.artist.name if album.artist else 'Unknown',
            'artist_id': album.artist.id if album.artist else None,
            'num_tracks': album.num_tracks,
            'duration': album.duration,
            'year': album.year,
            'release_date': str(album.release_date) if getattr(album, 'release_date', None) else '',
            'audio_quality': getattr(album, 'audio_quality', ''),
            'explicit': getattr(album, 'explicit', False),
            'image_url': album.image(640) if album else '',
            'image_url_small': album.image(160) if album else '',
        }

    def _artist_to_dict(self, artist):
        return {
            'id': artist.id,
            'name': artist.name,
            'image_url': artist.image(480) if artist else '',
        }

    def _playlist_to_dict(self, playlist):
        return {
            'id': playlist.id,
            'title': playlist.name,
            'description': getattr(playlist, 'description', ''),
            'num_tracks': playlist.num_tracks,
            'duration': playlist.duration,
            'image_url': playlist.image(320) if playlist else '',
        }

    def _mix_to_dict(self, mix):
        return {
            'id': mix.id,
            'title': mix.title or '',
            'sub_title': getattr(mix, 'sub_title', '') or '',
            'image_url': '',  # Caller fills this in when available
        }


class SocketServer:
    def __init__(self, tidal: TidalService):
        self.tidal = tidal

    async def handle_client(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter):
        log.info("Client connected")
        try:
            while True:
                line = await reader.readline()
                if not line:
                    break

                try:
                    request = json.loads(line.decode().strip())
                    response = await self.handle_request(request)
                except json.JSONDecodeError as e:
                    response = {'ok': False, 'error': f'Invalid JSON: {e}'}

                writer.write((json.dumps(response) + '\n').encode())
                await writer.drain()
        except (ConnectionResetError, BrokenPipeError):
            pass
        finally:
            log.info("Client disconnected")
            writer.close()

    async def handle_request(self, req):
        cmd = req.get('cmd', '')
        log.info(f"Command: {cmd}")

        try:
            if cmd == 'auth_status':
                logged_in = self.tidal.is_logged_in()
                user_name = ''
                if logged_in and self.tidal.session.user:
                    try:
                        user_name = self.tidal.session.user.name or ''
                    except:
                        pass
                return {'cmd': cmd, 'ok': True, 'logged_in': logged_in, 'user': user_name}

            elif cmd == 'auth_login':
                info = self.tidal.start_device_login()
                return {'cmd': cmd, 'ok': True, 'data': info}

            elif cmd == 'auth_check':
                done = self.tidal.check_device_login()
                return {'cmd': cmd, 'ok': True, 'logged_in': done}

            elif cmd == 'search':
                if not self.tidal.is_logged_in():
                    return {'cmd': cmd, 'ok': False, 'error': 'Not logged in'}
                results = self.tidal.search(
                    req.get('query', ''),
                    req.get('type', 'tracks'),
                    req.get('limit', 20),
                    req.get('offset', 0),
                )
                return {'cmd': cmd, 'ok': True, 'data': results}

            elif cmd == 'play_track':
                if not self.tidal.is_logged_in():
                    return {'cmd': cmd, 'ok': False, 'error': 'Not logged in'}
                stream = self.tidal.get_track_stream_url(req.get('track_id'))
                if stream:
                    return {'cmd': cmd, 'ok': True, 'data': stream}
                return {'cmd': cmd, 'ok': False, 'error': 'Failed to get stream URL'}

            elif cmd == 'get_album':
                if not self.tidal.is_logged_in():
                    return {'cmd': cmd, 'ok': False, 'error': 'Not logged in'}
                data = self.tidal.get_album_tracks(req.get('album_id'))
                return {'cmd': cmd, 'ok': True, 'data': data}

            elif cmd == 'get_artist':
                if not self.tidal.is_logged_in():
                    return {'cmd': cmd, 'ok': False, 'error': 'Not logged in'}
                data = self.tidal.get_artist_info(req.get('artist_id'))
                return {'cmd': cmd, 'ok': True, 'data': data}

            elif cmd == 'get_playlist':
                if not self.tidal.is_logged_in():
                    return {'cmd': cmd, 'ok': False, 'error': 'Not logged in'}
                data = self.tidal.get_playlist_tracks(req.get('playlist_id'))
                return {'cmd': cmd, 'ok': True, 'data': data}

            elif cmd == 'favorites':
                if not self.tidal.is_logged_in():
                    return {'cmd': cmd, 'ok': False, 'error': 'Not logged in'}
                data = self.tidal.get_favorites()
                return {'cmd': cmd, 'ok': True, 'data': data}

            elif cmd == 'add_favorite':
                if not self.tidal.is_logged_in():
                    return {'cmd': cmd, 'ok': False, 'error': 'Not logged in'}
                success = self.tidal.add_favorite(req.get('track_id'))
                return {'cmd': cmd, 'ok': success, 'error': '' if success else 'Failed to add favorite'}

            elif cmd == 'remove_favorite':
                if not self.tidal.is_logged_in():
                    return {'cmd': cmd, 'ok': False, 'error': 'Not logged in'}
                success = self.tidal.remove_favorite(req.get('track_id'))
                return {'cmd': cmd, 'ok': success, 'error': '' if success else 'Failed to remove favorite'}

            elif cmd == 'home':
                if not self.tidal.is_logged_in():
                    return {'cmd': cmd, 'ok': False, 'error': 'Not logged in'}
                data = self.tidal.get_home()
                return {'cmd': cmd, 'ok': True, 'data': data}

            elif cmd == 'get_mix':
                if not self.tidal.is_logged_in():
                    return {'cmd': cmd, 'ok': False, 'error': 'Not logged in'}
                data = self.tidal.get_mix_tracks(req.get('mix_id', ''))
                return {'cmd': cmd, 'ok': True, 'data': data}

            elif cmd == 'ping':
                return {'cmd': cmd, 'ok': True}

            else:
                return {'cmd': cmd, 'ok': False, 'error': f'Unknown command: {cmd}'}

        except Exception as e:
            log.error(f"Error handling {cmd}: {e}", exc_info=True)
            return {'cmd': cmd, 'ok': False, 'error': str(e)}

    async def run(self):
        # Clean up stale socket
        if os.path.exists(SOCKET_PATH):
            os.unlink(SOCKET_PATH)

        server = await asyncio.start_unix_server(
            self.handle_client, path=SOCKET_PATH
        )
        os.chmod(SOCKET_PATH, 0o660)

        log.info(f"Tidal service listening on {SOCKET_PATH}")

        async with server:
            await server.serve_forever()


async def main():
    tidal = TidalService()

    # Try to restore previous session
    tidal.load_session()

    if tidal.is_logged_in():
        log.info("Already logged in to Tidal")
    else:
        log.info("Not logged in - client will need to initiate auth flow")

    server = SocketServer(tidal)

    # Handle shutdown gracefully
    loop = asyncio.get_event_loop()
    for sig in (signal.SIGINT, signal.SIGTERM):
        loop.add_signal_handler(sig, lambda: asyncio.ensure_future(shutdown(server)))

    await server.run()


async def shutdown(server):
    log.info("Shutting down...")
    if os.path.exists(SOCKET_PATH):
        os.unlink(SOCKET_PATH)
    asyncio.get_event_loop().stop()


if __name__ == '__main__':
    asyncio.run(main())
