#!/usr/bin/env python3
"""
Spotify Music Service for HeadUnit.

Provides a JSON-based Unix socket server that the Qt6/C++ app connects to.
Handles Spotify authentication (PKCE), search, browsing, and playback
control via Spotify Connect (librespot).

Protocol: newline-delimited JSON messages over a Unix domain socket.
Request:  {"cmd": "search", "query": "radiohead", "type": "tracks", "limit": 20}
Response: {"cmd": "search", "ok": true, "data": [...]}

Commands:
  auth_status    - Check if logged in
  auth_login     - Start OAuth PKCE flow (returns auth URL)
  auth_callback  - Complete auth with redirect code
  search         - Search tracks/albums/artists/playlists
  play_track     - Play a track on librespot via Connect API
  play_context   - Play an album/playlist context
  pause          - Pause playback
  resume         - Resume playback
  next           - Skip to next
  previous       - Go to previous
  seek           - Seek to position
  get_album      - Get album tracks
  get_artist     - Get artist top tracks/albums
  get_playlist   - Get playlist tracks
  favorites      - Get saved/liked tracks
  add_favorite   - Save a track
  remove_favorite - Unsave a track
  now_playing    - Get current playback state
  devices        - List available Spotify Connect devices
  ping           - Health check
"""

import asyncio
import json
import os
import sys
import signal
import subprocess
import logging
import time
import threading
from pathlib import Path
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs

import spotipy
from spotipy.oauth2 import SpotifyOAuth

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [SpotifyService] %(levelname)s: %(message)s',
    datefmt='%H:%M:%S'
)
log = logging.getLogger('spotify')

SOCKET_PATH = '/tmp/headunit_spotify.sock'
SESSION_DIR = os.path.expanduser('~/.config/headunit')
CACHE_PATH = os.path.join(SESSION_DIR, '.spotify_cache')
LIBRESPOT_BIN = os.path.join(os.path.dirname(os.path.dirname(__file__)), 'external', 'librespot')

# Spotify OAuth settings
SPOTIFY_CLIENT_ID = os.environ.get('SPOTIFY_CLIENT_ID', '')
REDIRECT_URI = 'http://localhost:8888/callback'
SCOPES = ' '.join([
    'user-read-playback-state',
    'user-modify-playback-state',
    'user-read-currently-playing',
    'user-library-read',
    'user-library-modify',
    'playlist-read-private',
    'playlist-read-collaborative',
    'streaming',
])


class AuthCallbackHandler(BaseHTTPRequestHandler):
    """Minimal HTTP server to catch OAuth redirect callback."""
    auth_code = None

    def do_GET(self):
        parsed = urlparse(self.path)
        params = parse_qs(parsed.query)
        if 'code' in params:
            AuthCallbackHandler.auth_code = params['code'][0]
            self.send_response(200)
            self.send_header('Content-Type', 'text/html')
            self.end_headers()
            self.wfile.write(b'<html><body><h2>Login successful!</h2>'
                             b'<p>You can close this window and return to HeadUnit.</p>'
                             b'</body></html>')
        elif 'error' in params:
            self.send_response(400)
            self.send_header('Content-Type', 'text/html')
            self.end_headers()
            self.wfile.write(f'<html><body><h2>Error: {params["error"][0]}</h2></body></html>'.encode())
        else:
            self.send_response(404)
            self.end_headers()

    def log_message(self, format, *args):
        pass  # Suppress HTTP request logs


class SpotifyService:
    def __init__(self):
        os.makedirs(SESSION_DIR, exist_ok=True)

        self._sp = None  # spotipy.Spotify instance
        self._auth_manager = None
        self._auth_url = None
        self._auth_server = None
        self._auth_thread = None
        self._device_id = None  # librespot device ID
        self._device_name = 'HeadUnit'
        self._librespot_proc = None

        self._init_auth_manager()

    def _init_auth_manager(self):
        """Initialize the OAuth manager."""
        if not SPOTIFY_CLIENT_ID:
            log.warning("SPOTIFY_CLIENT_ID not set - auth will fail")
            return

        self._auth_manager = SpotifyOAuth(
            client_id=SPOTIFY_CLIENT_ID,
            redirect_uri=REDIRECT_URI,
            scope=SCOPES,
            cache_path=CACHE_PATH,
            open_browser=False,
        )

    def load_session(self):
        """Try to restore a cached session."""
        if not self._auth_manager:
            return False
        try:
            token_info = self._auth_manager.get_cached_token()
            if token_info:
                self._sp = spotipy.Spotify(auth_manager=self._auth_manager)
                # Verify token works
                self._sp.current_user()
                log.info("Session restored from cache")
                return True
        except Exception as e:
            log.warning(f"Failed to restore session: {e}")
        return False

    def is_logged_in(self):
        if not self._sp:
            return False
        try:
            # Refresh if needed
            token_info = self._auth_manager.get_cached_token()
            if token_info and self._auth_manager.is_token_expired(token_info):
                token_info = self._auth_manager.refresh_access_token(token_info['refresh_token'])
                self._sp = spotipy.Spotify(auth_manager=self._auth_manager)
            return token_info is not None
        except Exception:
            return False

    def get_user_name(self):
        if not self._sp:
            return ''
        try:
            user = self._sp.current_user()
            return user.get('display_name', '') or user.get('id', '')
        except Exception:
            return ''

    def start_auth(self):
        """Start OAuth PKCE flow. Returns the auth URL for the user to visit."""
        if not self._auth_manager:
            return None

        self._auth_url = self._auth_manager.get_authorize_url()

        # Start local callback server
        AuthCallbackHandler.auth_code = None
        try:
            self._auth_server = HTTPServer(('localhost', 8888), AuthCallbackHandler)
            self._auth_thread = threading.Thread(target=self._auth_server.handle_request, daemon=True)
            self._auth_thread.start()
        except OSError as e:
            log.warning(f"Could not start auth callback server: {e}")

        log.info(f"Auth URL: {self._auth_url}")
        return self._auth_url

    def check_auth(self):
        """Check if the OAuth callback has been received."""
        if AuthCallbackHandler.auth_code:
            code = AuthCallbackHandler.auth_code
            AuthCallbackHandler.auth_code = None
            try:
                token_info = self._auth_manager.get_access_token(code, as_dict=True)
                if token_info:
                    self._sp = spotipy.Spotify(auth_manager=self._auth_manager)
                    log.info("Login successful!")
                    # Clean up server
                    if self._auth_server:
                        self._auth_server = None
                    return True
            except Exception as e:
                log.error(f"Auth callback failed: {e}")
        return False

    # ── librespot management ──

    def start_librespot(self):
        """Start the librespot process (Spotify Connect receiver)."""
        if self._librespot_proc and self._librespot_proc.poll() is None:
            log.info("librespot already running")
            return True

        if not os.path.exists(LIBRESPOT_BIN):
            log.warning(f"librespot binary not found at {LIBRESPOT_BIN}")
            return False

        try:
            cmd = [
                LIBRESPOT_BIN,
                '--name', self._device_name,
                '--backend', 'alsa',
                '--bitrate', '320',
                '--device-type', 'computer',
                '--disable-audio-cache',
            ]

            # Pass credentials if we have a token
            token_info = self._auth_manager.get_cached_token() if self._auth_manager else None
            if token_info and token_info.get('access_token'):
                # librespot can use username/password or discover via zeroconf
                # For now, rely on Spotify Connect discovery
                pass

            self._librespot_proc = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
            log.info(f"librespot started (PID: {self._librespot_proc.pid})")
            return True
        except Exception as e:
            log.error(f"Failed to start librespot: {e}")
            return False

    def stop_librespot(self):
        """Stop the librespot process."""
        if self._librespot_proc:
            self._librespot_proc.terminate()
            try:
                self._librespot_proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self._librespot_proc.kill()
            self._librespot_proc = None
            log.info("librespot stopped")

    def find_device(self):
        """Find the librespot device in Spotify Connect device list."""
        if not self._sp:
            return None
        try:
            devices = self._sp.devices()
            for d in devices.get('devices', []):
                if d.get('name') == self._device_name:
                    self._device_id = d['id']
                    log.info(f"Found device: {self._device_name} ({self._device_id})")
                    return self._device_id
        except Exception as e:
            log.warning(f"Device lookup failed: {e}")
        return None

    # ── Playback Control (via Spotify Connect API) ──

    def play_track(self, track_uri, context_uri=None):
        """Play a track on the librespot device."""
        if not self._sp:
            return False
        device = self._device_id or self.find_device()
        if not device:
            log.warning("No HeadUnit device found - trying anyway")

        try:
            if context_uri:
                self._sp.start_playback(
                    device_id=device,
                    context_uri=context_uri,
                    offset={'uri': track_uri}
                )
            else:
                self._sp.start_playback(
                    device_id=device,
                    uris=[track_uri]
                )
            return True
        except Exception as e:
            log.error(f"Play failed: {e}")
            return False

    def play_context(self, context_uri, offset=0):
        """Play an album/playlist context."""
        if not self._sp:
            return False
        device = self._device_id or self.find_device()
        try:
            self._sp.start_playback(
                device_id=device,
                context_uri=context_uri,
                offset={'position': offset}
            )
            return True
        except Exception as e:
            log.error(f"Play context failed: {e}")
            return False

    def play_tracks(self, uris):
        """Play a list of track URIs."""
        if not self._sp or not uris:
            return False
        device = self._device_id or self.find_device()
        try:
            self._sp.start_playback(device_id=device, uris=uris)
            return True
        except Exception as e:
            log.error(f"Play tracks failed: {e}")
            return False

    def pause(self):
        if not self._sp:
            return False
        try:
            self._sp.pause_playback(device_id=self._device_id)
            return True
        except Exception as e:
            log.warning(f"Pause failed: {e}")
            return False

    def resume(self):
        if not self._sp:
            return False
        try:
            self._sp.start_playback(device_id=self._device_id)
            return True
        except Exception as e:
            log.warning(f"Resume failed: {e}")
            return False

    def next_track(self):
        if not self._sp:
            return False
        try:
            self._sp.next_track(device_id=self._device_id)
            return True
        except Exception as e:
            log.warning(f"Next failed: {e}")
            return False

    def previous_track(self):
        if not self._sp:
            return False
        try:
            self._sp.previous_track(device_id=self._device_id)
            return True
        except Exception as e:
            log.warning(f"Previous failed: {e}")
            return False

    def seek(self, position_ms):
        if not self._sp:
            return False
        try:
            self._sp.seek_track(position_ms, device_id=self._device_id)
            return True
        except Exception as e:
            log.warning(f"Seek failed: {e}")
            return False

    def set_shuffle(self, enabled):
        if not self._sp:
            return False
        try:
            self._sp.shuffle(enabled, device_id=self._device_id)
            return True
        except Exception as e:
            log.warning(f"Shuffle failed: {e}")
            return False

    def set_repeat(self, mode):
        """mode: 'off', 'track', 'context'"""
        if not self._sp:
            return False
        try:
            self._sp.repeat(mode, device_id=self._device_id)
            return True
        except Exception as e:
            log.warning(f"Repeat failed: {e}")
            return False

    # ── Now Playing / Playback State ──

    def get_playback_state(self):
        """Get current playback state from Spotify."""
        if not self._sp:
            return None
        try:
            pb = self._sp.current_playback()
            if not pb:
                return {'is_playing': False}

            track = pb.get('item')
            result = {
                'is_playing': pb.get('is_playing', False),
                'progress_ms': pb.get('progress_ms', 0),
                'shuffle': pb.get('shuffle_state', False),
                'repeat': pb.get('repeat_state', 'off'),
                'device': pb.get('device', {}).get('name', ''),
            }

            if track:
                artists = track.get('artists', [])
                album = track.get('album', {})
                images = album.get('images', [])
                result.update({
                    'track_uri': track.get('uri', ''),
                    'track_id': track.get('id', ''),
                    'title': track.get('name', ''),
                    'artist': ', '.join(a.get('name', '') for a in artists),
                    'artist_id': artists[0].get('id', '') if artists else '',
                    'album': album.get('name', ''),
                    'album_id': album.get('id', ''),
                    'album_uri': album.get('uri', ''),
                    'duration_ms': track.get('duration_ms', 0),
                    'explicit': track.get('explicit', False),
                    'image_url': images[0]['url'] if images else '',
                    'image_url_small': images[-1]['url'] if images else '',
                })
            return result
        except Exception as e:
            log.warning(f"Playback state failed: {e}")
            return None

    def get_devices(self):
        """List available Spotify Connect devices."""
        if not self._sp:
            return []
        try:
            result = self._sp.devices()
            return [
                {
                    'id': d['id'],
                    'name': d['name'],
                    'type': d['type'],
                    'is_active': d['is_active'],
                    'volume': d.get('volume_percent', 0),
                }
                for d in result.get('devices', [])
            ]
        except Exception as e:
            log.warning(f"Devices failed: {e}")
            return []

    # ── Search & Browse ──

    def search(self, query, search_type='track', limit=20, offset=0):
        """Search Spotify catalog."""
        if not self._sp:
            return []
        try:
            results = self._sp.search(q=query, type=search_type, limit=limit, offset=offset)

            if search_type == 'track':
                return [self._track_to_dict(t) for t in results.get('tracks', {}).get('items', [])]
            elif search_type == 'album':
                return [self._album_to_dict(a) for a in results.get('albums', {}).get('items', [])]
            elif search_type == 'artist':
                return [self._artist_to_dict(a) for a in results.get('artists', {}).get('items', [])]
            elif search_type == 'playlist':
                return [self._playlist_to_dict(p) for p in results.get('playlists', {}).get('items', [])]
            return []
        except Exception as e:
            log.error(f"Search failed: {e}")
            return []

    def get_album_tracks(self, album_id):
        """Get album info and tracks."""
        if not self._sp:
            return None
        try:
            album = self._sp.album(album_id)
            tracks = self._sp.album_tracks(album_id, limit=50)
            # Album tracks don't include album info, merge it
            album_dict = self._album_to_dict(album)
            track_list = []
            for t in tracks.get('items', []):
                td = self._track_to_dict(t)
                td['album'] = album_dict['title']
                td['album_id'] = album_id
                td['image_url'] = album_dict['image_url']
                td['image_url_small'] = album_dict['image_url_small']
                track_list.append(td)
            return {'album': album_dict, 'tracks': track_list}
        except Exception as e:
            log.error(f"Get album failed: {e}")
            return None

    def get_artist_info(self, artist_id):
        """Get artist top tracks and albums."""
        if not self._sp:
            return None
        try:
            artist = self._sp.artist(artist_id)
            top_tracks = self._sp.artist_top_tracks(artist_id)
            albums = self._sp.artist_albums(artist_id, album_type='album', limit=20)
            return {
                'artist': self._artist_to_dict(artist),
                'top_tracks': [self._track_to_dict(t) for t in top_tracks.get('tracks', [])],
                'albums': [self._album_to_dict(a) for a in albums.get('items', [])],
            }
        except Exception as e:
            log.error(f"Get artist failed: {e}")
            return None

    def get_playlist_tracks(self, playlist_id):
        """Get playlist info and tracks."""
        if not self._sp:
            return None
        try:
            playlist = self._sp.playlist(playlist_id)
            tracks = []
            for item in playlist.get('tracks', {}).get('items', []):
                t = item.get('track')
                if t and t.get('id'):  # Skip local/unavailable tracks
                    tracks.append(self._track_to_dict(t))
            return {
                'playlist': self._playlist_to_dict(playlist),
                'tracks': tracks,
            }
        except Exception as e:
            log.error(f"Get playlist failed: {e}")
            return None

    def get_favorites(self, limit=50, offset=0):
        """Get user's saved/liked tracks."""
        if not self._sp:
            return []
        try:
            results = self._sp.current_user_saved_tracks(limit=limit, offset=offset)
            return [self._track_to_dict(item['track']) for item in results.get('items', [])
                    if item.get('track') and item['track'].get('id')]
        except Exception as e:
            log.error(f"Get favorites failed: {e}")
            return []

    def get_user_playlists(self, limit=50):
        """Get user's playlists."""
        if not self._sp:
            return []
        try:
            results = self._sp.current_user_playlists(limit=limit)
            return [self._playlist_to_dict(p) for p in results.get('items', []) if p]
        except Exception as e:
            log.error(f"Get playlists failed: {e}")
            return []

    def add_favorite(self, track_id):
        if not self._sp:
            return False
        try:
            self._sp.current_user_saved_tracks_add([track_id])
            log.info(f"Saved track {track_id}")
            return True
        except Exception as e:
            log.error(f"Save track failed: {e}")
            return False

    def remove_favorite(self, track_id):
        if not self._sp:
            return False
        try:
            self._sp.current_user_saved_tracks_delete([track_id])
            log.info(f"Unsaved track {track_id}")
            return True
        except Exception as e:
            log.error(f"Unsave track failed: {e}")
            return False

    # ── Helpers ──

    def _track_to_dict(self, track):
        artists = track.get('artists', [])
        album = track.get('album', {})
        images = album.get('images', [])
        return {
            'id': track.get('id', ''),
            'uri': track.get('uri', ''),
            'title': track.get('name', ''),
            'artist': ', '.join(a.get('name', '') for a in artists),
            'artist_id': artists[0].get('id', '') if artists else '',
            'album': album.get('name', ''),
            'album_id': album.get('id', ''),
            'album_uri': album.get('uri', ''),
            'duration': track.get('duration_ms', 0) // 1000,
            'duration_ms': track.get('duration_ms', 0),
            'track_num': track.get('track_number', 0),
            'explicit': track.get('explicit', False),
            'image_url': images[0]['url'] if images else '',
            'image_url_small': images[-1]['url'] if len(images) > 1 else (images[0]['url'] if images else ''),
        }

    def _album_to_dict(self, album):
        artists = album.get('artists', [])
        images = album.get('images', [])
        return {
            'id': album.get('id', ''),
            'uri': album.get('uri', ''),
            'title': album.get('name', ''),
            'artist': ', '.join(a.get('name', '') for a in artists),
            'artist_id': artists[0].get('id', '') if artists else '',
            'num_tracks': album.get('total_tracks', 0),
            'release_date': album.get('release_date', ''),
            'image_url': images[0]['url'] if images else '',
            'image_url_small': images[-1]['url'] if len(images) > 1 else (images[0]['url'] if images else ''),
        }

    def _artist_to_dict(self, artist):
        images = artist.get('images', [])
        return {
            'id': artist.get('id', ''),
            'uri': artist.get('uri', ''),
            'name': artist.get('name', ''),
            'image_url': images[0]['url'] if images else '',
            'followers': artist.get('followers', {}).get('total', 0),
        }

    def _playlist_to_dict(self, playlist):
        images = playlist.get('images', [])
        return {
            'id': playlist.get('id', ''),
            'uri': playlist.get('uri', ''),
            'title': playlist.get('name', ''),
            'description': playlist.get('description', ''),
            'num_tracks': playlist.get('tracks', {}).get('total', 0),
            'image_url': images[0]['url'] if images else '',
            'owner': playlist.get('owner', {}).get('display_name', ''),
        }


class SocketServer:
    def __init__(self, spotify: SpotifyService):
        self.spotify = spotify

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
                logged_in = self.spotify.is_logged_in()
                user_name = self.spotify.get_user_name() if logged_in else ''
                return {'cmd': cmd, 'ok': True, 'logged_in': logged_in, 'user': user_name}

            elif cmd == 'auth_login':
                url = self.spotify.start_auth()
                if url:
                    return {'cmd': cmd, 'ok': True, 'data': {'auth_url': url}}
                return {'cmd': cmd, 'ok': False, 'error': 'Failed to start auth (SPOTIFY_CLIENT_ID set?)'}

            elif cmd == 'auth_check':
                done = self.spotify.check_auth()
                return {'cmd': cmd, 'ok': True, 'logged_in': done}

            elif cmd == 'search':
                if not self.spotify.is_logged_in():
                    return {'cmd': cmd, 'ok': False, 'error': 'Not logged in'}
                # Spotify search types are singular: track, album, artist, playlist
                search_type = req.get('type', 'tracks')
                # Normalize: "tracks" -> "track" for spotipy
                if search_type.endswith('s'):
                    search_type = search_type[:-1]
                results = self.spotify.search(
                    req.get('query', ''),
                    search_type,
                    req.get('limit', 20),
                    req.get('offset', 0),
                )
                return {'cmd': cmd, 'ok': True, 'data': results}

            elif cmd == 'play_track':
                if not self.spotify.is_logged_in():
                    return {'cmd': cmd, 'ok': False, 'error': 'Not logged in'}
                uri = req.get('uri', '')
                context = req.get('context_uri', '')
                if not uri:
                    track_id = req.get('track_id', '')
                    if track_id:
                        uri = f'spotify:track:{track_id}'
                success = self.spotify.play_track(uri, context or None)
                return {'cmd': cmd, 'ok': success, 'error': '' if success else 'Playback failed'}

            elif cmd == 'play_tracks':
                if not self.spotify.is_logged_in():
                    return {'cmd': cmd, 'ok': False, 'error': 'Not logged in'}
                uris = req.get('uris', [])
                success = self.spotify.play_tracks(uris)
                return {'cmd': cmd, 'ok': success}

            elif cmd == 'play_context':
                if not self.spotify.is_logged_in():
                    return {'cmd': cmd, 'ok': False, 'error': 'Not logged in'}
                success = self.spotify.play_context(
                    req.get('context_uri', ''),
                    req.get('offset', 0),
                )
                return {'cmd': cmd, 'ok': success}

            elif cmd == 'pause':
                success = self.spotify.pause()
                return {'cmd': cmd, 'ok': success}

            elif cmd == 'resume':
                success = self.spotify.resume()
                return {'cmd': cmd, 'ok': success}

            elif cmd == 'next':
                success = self.spotify.next_track()
                return {'cmd': cmd, 'ok': success}

            elif cmd == 'previous':
                success = self.spotify.previous_track()
                return {'cmd': cmd, 'ok': success}

            elif cmd == 'seek':
                success = self.spotify.seek(req.get('position_ms', 0))
                return {'cmd': cmd, 'ok': success}

            elif cmd == 'shuffle':
                success = self.spotify.set_shuffle(req.get('enabled', False))
                return {'cmd': cmd, 'ok': success}

            elif cmd == 'repeat':
                success = self.spotify.set_repeat(req.get('mode', 'off'))
                return {'cmd': cmd, 'ok': success}

            elif cmd == 'now_playing':
                state = self.spotify.get_playback_state()
                if state:
                    return {'cmd': cmd, 'ok': True, 'data': state}
                return {'cmd': cmd, 'ok': True, 'data': {'is_playing': False}}

            elif cmd == 'devices':
                devices = self.spotify.get_devices()
                return {'cmd': cmd, 'ok': True, 'data': devices}

            elif cmd == 'get_album':
                if not self.spotify.is_logged_in():
                    return {'cmd': cmd, 'ok': False, 'error': 'Not logged in'}
                data = self.spotify.get_album_tracks(req.get('album_id', ''))
                if data:
                    return {'cmd': cmd, 'ok': True, 'data': data}
                return {'cmd': cmd, 'ok': False, 'error': 'Album not found'}

            elif cmd == 'get_artist':
                if not self.spotify.is_logged_in():
                    return {'cmd': cmd, 'ok': False, 'error': 'Not logged in'}
                data = self.spotify.get_artist_info(req.get('artist_id', ''))
                if data:
                    return {'cmd': cmd, 'ok': True, 'data': data}
                return {'cmd': cmd, 'ok': False, 'error': 'Artist not found'}

            elif cmd == 'get_playlist':
                if not self.spotify.is_logged_in():
                    return {'cmd': cmd, 'ok': False, 'error': 'Not logged in'}
                data = self.spotify.get_playlist_tracks(req.get('playlist_id', ''))
                if data:
                    return {'cmd': cmd, 'ok': True, 'data': data}
                return {'cmd': cmd, 'ok': False, 'error': 'Playlist not found'}

            elif cmd == 'favorites':
                if not self.spotify.is_logged_in():
                    return {'cmd': cmd, 'ok': False, 'error': 'Not logged in'}
                data = self.spotify.get_favorites(
                    req.get('limit', 50),
                    req.get('offset', 0),
                )
                return {'cmd': cmd, 'ok': True, 'data': data}

            elif cmd == 'playlists':
                if not self.spotify.is_logged_in():
                    return {'cmd': cmd, 'ok': False, 'error': 'Not logged in'}
                data = self.spotify.get_user_playlists()
                return {'cmd': cmd, 'ok': True, 'data': data}

            elif cmd == 'add_favorite':
                if not self.spotify.is_logged_in():
                    return {'cmd': cmd, 'ok': False, 'error': 'Not logged in'}
                success = self.spotify.add_favorite(req.get('track_id', ''))
                return {'cmd': cmd, 'ok': success}

            elif cmd == 'remove_favorite':
                if not self.spotify.is_logged_in():
                    return {'cmd': cmd, 'ok': False, 'error': 'Not logged in'}
                success = self.spotify.remove_favorite(req.get('track_id', ''))
                return {'cmd': cmd, 'ok': success}

            elif cmd == 'start_librespot':
                success = self.spotify.start_librespot()
                return {'cmd': cmd, 'ok': success}

            elif cmd == 'find_device':
                device = self.spotify.find_device()
                return {'cmd': cmd, 'ok': device is not None, 'device_id': device or ''}

            elif cmd == 'ping':
                return {'cmd': cmd, 'ok': True}

            else:
                return {'cmd': cmd, 'ok': False, 'error': f'Unknown command: {cmd}'}

        except Exception as e:
            log.error(f"Error handling {cmd}: {e}", exc_info=True)
            return {'cmd': cmd, 'ok': False, 'error': str(e)}

    async def run(self):
        if os.path.exists(SOCKET_PATH):
            os.unlink(SOCKET_PATH)

        server = await asyncio.start_unix_server(
            self.handle_client, path=SOCKET_PATH
        )
        os.chmod(SOCKET_PATH, 0o660)
        log.info(f"Spotify service listening on {SOCKET_PATH}")

        async with server:
            await server.serve_forever()


async def main():
    spotify = SpotifyService()

    # Try to restore previous session
    spotify.load_session()

    if spotify.is_logged_in():
        log.info("Already logged in to Spotify")
        # Start librespot if available
        spotify.start_librespot()
        # Try to find device
        spotify.find_device()
    else:
        log.info("Not logged in - client will need to initiate auth flow")

    server = SocketServer(spotify)

    loop = asyncio.get_event_loop()
    for sig in (signal.SIGINT, signal.SIGTERM):
        loop.add_signal_handler(sig, lambda: asyncio.ensure_future(shutdown(server, spotify)))

    await server.run()


async def shutdown(server, spotify):
    log.info("Shutting down...")
    spotify.stop_librespot()
    if os.path.exists(SOCKET_PATH):
        os.unlink(SOCKET_PATH)
    asyncio.get_event_loop().stop()


if __name__ == '__main__':
    asyncio.run(main())
