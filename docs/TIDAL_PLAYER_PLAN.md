# Tidal Player Implementation Plan — HeadUnit

## Design Philosophy

Tidal's signature: **black stage, white typography, cyan accents** — but our HeadUnit uses a theme system, so we adapt Tidal's *layout patterns* while respecting the active theme's colors. The player must be **automotive-safe**: NHTSA 2-second glance rule, 76px minimum touch targets, 24px minimum text, and no distracting animations.

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│                    Main.qml                             │
│  activeAudioSource: "tidal" | "phone" | "none"          │
│                                                         │
│  ┌─────────┐  ┌──────────────────────────┐  ┌────────┐ │
│  │ NavBar  │  │    ScreenContainer       │  │        │ │
│  │         │  │  ┌────────────────────┐   │  │        │ │
│  │         │  │  │   Tidal.qml        │   │  │        │ │
│  │         │  │  │                    │   │  │        │ │
│  │         │  │  │  Search / Browse   │   │  │        │ │
│  │         │  │  │  Now Playing       │   │  │        │ │
│  │         │  │  │  Queue             │   │  │        │ │
│  │         │  │  └────────────────────┘   │  │        │ │
│  │         │  └──────────────────────────┘  │        │ │
│  └─────────┘  ┌──────────────────────────┐  └────────┘ │
│               │      MiniPlayer          │              │
│               │  (Tidal OR Bluetooth)    │              │
│               └──────────────────────────┘              │
└─────────────────────────────────────────────────────────┘

C++ Layer:
  TidalClient ←→ QLocalSocket ←→ tidal_service.py ←→ Tidal API
  TidalClient → GStreamer playbin → PulseAudio → speakers
```

---

## Phase 1: Audio Playback Pipeline

**Goal**: When user taps a track, it actually plays audio.

### 1.1 GStreamer Integration in TidalClient

Add GStreamer playbin to TidalClient.cpp for streaming audio from URLs.

**New properties:**
```
Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY playStateChanged)
Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)     // ms
Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)     // ms
Q_PROPERTY(QString audioQuality READ audioQuality NOTIFY trackChanged)
```

**New slots:**
```
void play()
void pause()
void resume()
void stop()
void seekTo(qint64 positionMs)
void next()          // play next in queue
void previous()      // play previous or restart
void setVolume(int percent)  // optional, amp controls volume
```

**Implementation:**
- Use `GstElement *playbin` with `playbin` factory
- On `streamReady(url)` → set `playbin` URI → set state to PLAYING
- Bus watch for EOS (end of stream) → auto-play next in queue
- Bus watch for state changes → update `isPlaying`
- Timer (200ms) polls position → emit `positionChanged`
- Parse duration from stream → emit `durationChanged`

**CMakeLists.txt addition:**
```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0)
target_include_directories(appHeadUnit PRIVATE ${GST_INCLUDE_DIRS})
target_link_libraries(appHeadUnit PRIVATE ${GST_LIBRARIES})
```

### 1.2 Queue Management in TidalClient

**New properties:**
```
Q_PROPERTY(QVariantList queue READ queue NOTIFY queueChanged)
Q_PROPERTY(int queuePosition READ queuePosition NOTIFY queuePositionChanged)
Q_PROPERTY(bool shuffleEnabled READ shuffleEnabled NOTIFY shuffleChanged)
Q_PROPERTY(int repeatMode READ repeatMode NOTIFY repeatModeChanged)  // 0=off, 1=all, 2=one
```

**New slots:**
```
void playTrackInContext(int trackId, QVariantList trackList, int index)
void addToQueue(QVariantList tracks)
void clearQueue()
void removeFromQueue(int index)
void toggleShuffle()
void cycleRepeatMode()
```

**Logic:**
- When user taps a track in search results / album / favorites, call `playTrackInContext()` with the full list and clicked index
- Queue = the track list; queuePosition = clicked index
- On EOS: advance queuePosition, request next stream URL, play
- Shuffle: Fisher-Yates on queue copy, maintain original order for unshuffle
- Repeat: 0=stop at end, 1=wrap to start, 2=replay current track

### 1.3 Now-Playing Track Metadata

When `play_track` response arrives, populate track metadata from the queue item (already have title/artist/album/image_url/duration from search/browse results). Update:
- `m_trackTitle`, `m_artist`, `m_album`, `m_albumArtUrl`, `m_trackDuration`
- Emit `trackChanged()`

---

## Phase 2: Tidal.qml — Full Redesign

### 2.0 Display Dimensions

Available content area: **~1440 x 620px** (1560 - 120 NavBar width, 720 - 80 MiniPlayer - 20 margins)

### 2.1 Screen States

The Tidal screen has **two major modes**, controlled by `tidalClient.isPlaying`:

**A) Browse Mode** (no track playing, or track playing but user is browsing)
**B) Now Playing Mode** (user taps now-playing area or "now playing" button)

Both share the same screen; Now Playing slides over Browse from the right.

### 2.2 Browse Mode Layout

```
┌──────────────────────────────────────────────────────┐
│ ← Back    [🔍 Search Tidal...                ] ● OK │  48px
├──────────────────────────────────────────────────────┤
│ [Tracks] [Albums] [Artists] [Favorites]              │  44px
├──────────────────────────────────────────────────────┤
│                                                      │
│  ┌──────┐  Track Title                    3:42  │▸│  │  76px per row
│  │ art  │  Artist Name · Album Name              │  │
│  │64x64 │                                        │  │
│  └──────┘                                        │  │
│  ─────────────────────────────────────────────────  │
│  ┌──────┐  Track Title                    4:15  │▸│  │
│  │ art  │  Artist Name · Album Name              │  │
│  └──────┘                                        │  │
│  ─────────────────────────────────────────────────  │
│  ┌──────┐  Track Title                    3:28  │▸│  │
│  │ art  │  Artist Name · Album Name              │  │
│  └──────┘                                        │  │
│                                                      │
│  ... (scrollable ListView)                           │
│                                                      │
└──────────────────────────────────────────────────────┘
```

**Key dimensions:**
- Search bar: full width minus back button and status, 48px height
- Tab buttons: 44px height, equal width, 8px spacing
- Track rows: **76px height** (automotive minimum touch target)
- Album art thumbnail: **64x64px**, 6px radius
- Track title: **fontSize (18px)**, bold
- Artist/album: **fontSize - 3 (15px)**, muted
- Duration: **fontSize - 3**, right-aligned
- Play indicator: small icon on right edge of row

### 2.3 Album Detail View

When user taps an album in search results:

```
┌──────────────────────────────────────────────────────┐
│ ← Back                                    ● Status   │  48px
├──────────────┬───────────────────────────────────────┤
│              │  Album Title                          │
│   ┌──────┐   │  Artist Name                          │
│   │      │   │  2024 · 12 tracks · 48 min            │
│   │ 200  │   │                                       │
│   │ x200 │   │  [▶ Play All]  [⇄ Shuffle]  [♡]      │
│   │      │   │                                       │
│   └──────┘   │                                       │
│   240px      │                                       │
├──────────────┴───────────────────────────────────────┤
│  1.  Track Title                           3:42      │  76px
│  2.  Track Title                           4:15      │
│  3.  Track Title (feat. Artist)            3:28      │
│  ...                                                 │
└──────────────────────────────────────────────────────┘
```

**Left column**: 240px — album art (200x200) with padding
**Right column**: remaining width — metadata + action buttons + track list
**Play All**: plays entire album starting from track 1, populates queue
**Shuffle**: plays album in random order
**Heart**: add/remove album from favorites

### 2.4 Artist Detail View

```
┌──────────────────────────────────────────────────────┐
│ ← Back                                    ● Status   │  48px
├──────────────┬───────────────────────────────────────┤
│              │  Artist Name                          │
│   ┌──────┐   │                                       │
│   │ 120  │   │  [▶ Play] [⇄ Shuffle] [♡ Follow]     │
│   │circle│   │                                       │
│   └──────┘   ├───────────────────────────────────────┤
│              │  TOP TRACKS                           │
│   160px      │  1. Track Title           3:42        │  76px
│              │  2. Track Title           4:15        │
│              │  3. Track Title           3:28        │
│              │  4. Track Title           5:01        │
│              │  5. Track Title           3:55        │
│              ├───────────────────────────────────────┤
│              │  ALBUMS                               │
│              │  ┌────┐ ┌────┐ ┌────┐ ┌────┐         │
│              │  │art │ │art │ │art │ │art │  horiz  │
│              │  └────┘ └────┘ └────┘ └────┘  scroll  │
│              │  Title   Title  Title  Title           │
└──────────────┴───────────────────────────────────────┘
```

### 2.5 Now Playing View (Full Screen Overlay)

When user taps a track to play, or taps the "Now Playing" indicator:

```
┌──────────────────────────────────────────────────────┐
│                                                      │
│    ┌─────────────────┐    Track Title                │
│    │                 │    Artist Name                 │
│    │                 │    Album Name                  │
│    │   Album Art     │                               │
│    │   280 x 280     │    ┌─── Quality Badge ───┐    │
│    │                 │    │  LOSSLESS · FLAC     │    │
│    │                 │    └──────────────────────┘    │
│    └─────────────────┘                               │
│                                                      │
│    1:23 ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ 4:15    │
│                                                      │
│         ⇄       ⏮      ▶ / ⏸      ⏭       🔁      │
│        64px    76px    96px     76px    64px          │
│                                                      │
│    [♡ Fav]                           [Queue ☰]       │
│                                                      │
└──────────────────────────────────────────────────────┘
```

**Layout**: Two-column for landscape
- **Left ~40%**: Album art, 280x280, with subtle border, theme-colored shadow
- **Right ~60%**: Track metadata, quality badge, progress bar, controls
- **Progress bar**: 20px height, full width of right column, tappable for seeking
- **Controls row**: centered, generous spacing (24px between buttons)
- **Play/Pause**: 96x96 touch target, theme primary color fill
- **Skip/Previous**: 76x76 touch target, outlined
- **Shuffle/Repeat**: 64x64, toggle state shown by opacity/color
- **Bottom row**: Favorite toggle (left), Queue button (right)

**Typography:**
- Track title: fontSize + 6 (24px), bold, white
- Artist: fontSize + 2 (20px), theme primary color, tappable
- Album: fontSize (18px), muted (60% opacity)
- Quality badge: fontSize - 4 (14px), pill shape with border
- Time stamps: fontSize - 2 (16px)

### 2.6 Queue Panel

Slides in from right when user taps Queue button in Now Playing:

```
┌───────────────────────────────────┐
│  NOW PLAYING                      │
│  ┌──┐ Current Track Title   3:42  │  highlighted row
│  └──┘ Artist                      │
├───────────────────────────────────┤
│  NEXT UP                          │
│  ┌──┐ Track 2              4:15   │  76px rows
│  └──┘ Artist                      │
│  ┌──┐ Track 3              3:28   │
│  └──┘ Artist                      │
│  ┌──┐ Track 4              5:01   │
│  └──┘ Artist                      │
│  ...                              │
├───────────────────────────────────┤
│  [Clear Queue]                    │
└───────────────────────────────────┘
```

Width: 400px, slides over right side of Now Playing view.

### 2.7 Login Screen (Keep Current)

The existing login screen is already well-designed:
- Large "TIDAL" branding
- "Sign In with Device Code" button
- URL + code display with pulsing "Waiting..." indicator
- Only change: increase button height to 64px (automotive target)

### 2.8 Home / Default View

When logged in but no search active, show:

```
┌──────────────────────────────────────────────────────┐
│ [🔍 Search Tidal...                        ] ● User  │  48px
├──────────────────────────────────────────────────────┤
│  MY MIXES                                            │
│  ┌────────┐ ┌────────┐ ┌────────┐ ┌────────┐        │
│  │ Mix 1  │ │ Mix 2  │ │ Mix 3  │ │ Mix 4  │  120px │
│  │ art    │ │ art    │ │ art    │ │ art    │ cards  │
│  └────────┘ └────────┘ └────────┘ └────────┘        │
│   Daily      Discovery   Artist     New Release      │
├──────────────────────────────────────────────────────┤
│  FAVORITES                           [See All →]     │
│  ┌──────┐  Track Title                    3:42       │
│  │ art  │  Artist · Album                            │  76px
│  └──────┘                                            │
│  ┌──────┐  Track Title                    4:15       │
│  │ art  │  Artist · Album                            │  76px
│  └──────┘                                            │
│  ... (first 5 favorites)                             │
└──────────────────────────────────────────────────────┘
```

---

## Phase 3: MiniPlayer Integration

### 3.1 Multi-Source MiniPlayer

MiniPlayer.qml needs to read from the active source:

```qml
// Current source detection
readonly property bool isTidalSource: mainWindow.activeAudioSource === "tidal"

// Metadata binding (source-aware)
readonly property string currentTrack:
    isTidalSource ? (tidalClient ? tidalClient.trackTitle : "")
                  : (mediaController ? mediaController.trackTitle : "No Track")

readonly property string currentArtist:
    isTidalSource ? (tidalClient ? tidalClient.artist : "")
                  : (mediaController ? mediaController.artist : "")

readonly property string currentAlbum:
    isTidalSource ? (tidalClient ? tidalClient.album : "")
                  : (mediaController ? mediaController.album : "")

readonly property bool isPlaying:
    isTidalSource ? (tidalClient ? tidalClient.isPlaying : false)
                  : (mediaController ? mediaController.isPlaying : false)

readonly property string appName:
    isTidalSource ? "Tidal"
                  : (mediaController ? mediaController.activeApp : "Music")
```

**Control forwarding:**
```qml
function doPlay()    { isTidalSource ? tidalClient.resume() : mediaController.play() }
function doPause()   { isTidalSource ? tidalClient.pause()  : mediaController.pause() }
function doNext()    { isTidalSource ? tidalClient.next()    : mediaController.next() }
function doPrevious(){ isTidalSource ? tidalClient.previous(): mediaController.previous() }
```

### 3.2 Source Badge in MiniPlayer

Show small "TIDAL" or "BLUETOOTH" text badge below the track info to indicate source.

### 3.3 Album Art in MiniPlayer

For Tidal: use `tidalClient.albumArtUrl` (HTTP URL from Tidal CDN, 320px size)
For Bluetooth: use `mediaController.albumArtUrl` (local path from AVRCP)

The existing MiniPlayer shows a music icon placeholder — replace with actual album art Image when URL is available.

---

## Phase 4: Main.qml Audio Source Wiring

### 4.1 Source Switching

```qml
// In Main.qml, extend setAudioSource:
function setAudioSource(source) {
    if (activeAudioSource === source) return
    console.log("Audio source:", activeAudioSource, "→", source)

    // Pause outgoing source
    if (activeAudioSource === "phone") mediaController.pause()
    if (activeAudioSource === "tidal") tidalClient.pause()

    activeAudioSource = source
}
```

### 4.2 Tidal Screen Triggers Source Switch

In Tidal.qml, when a track starts playing:
```qml
Connections {
    target: tidalClient
    function onPlayStateChanged() {
        if (tidalClient.isPlaying) {
            mainWindow.setAudioSource("tidal")
        }
    }
}
```

### 4.3 Bluetooth Reclaims on AVRCP Play

In Main.qml's existing mediaController connections:
```qml
function onPlayStateChanged() {
    if (mediaController.isPlaying) {
        mainWindow.setAudioSource("phone")
    }
}
```

---

## Phase 5: Python Service Enhancements

### 5.1 New Commands Needed

| Command | Purpose | Response |
|---------|---------|----------|
| `add_favorite` | Add track to user favorites | `{ok: true}` |
| `remove_favorite` | Remove track from favorites | `{ok: true}` |
| `get_album_detail` | Album with full metadata (year, quality) | Enhanced album data |
| `get_artist_detail` | Artist with bio, similar artists | Enhanced artist data |
| `get_mixes` | User's personal mixes with images | Mix cards data |

### 5.2 Enhanced Track Data

Current `_track_to_dict()` is missing fields. Add:
```python
def _track_to_dict(self, track):
    return {
        'id': track.id,
        'title': track.name,
        'version': getattr(track, 'version', ''),
        'artist': track.artist.name if track.artist else 'Unknown',
        'artist_id': track.artist.id if track.artist else None,
        'artists': [{'id': a.id, 'name': a.name} for a in getattr(track, 'artists', [])],
        'album': track.album.name if track.album else '',
        'album_id': track.album.id if track.album else None,
        'duration': track.duration,
        'track_num': track.track_num,
        'volume_num': getattr(track, 'volume_num', 1),
        'explicit': getattr(track, 'explicit', False),
        'audio_quality': getattr(track, 'audio_quality', ''),
        'audio_modes': getattr(track, 'audio_modes', []),
        'image_url': track.album.image(640) if track.album else '',
        'image_url_small': track.album.image(160) if track.album else '',
    }
```

### 5.3 Enhanced Album Data

```python
def _album_to_dict(self, album):
    return {
        'id': album.id,
        'title': album.name,
        'artist': album.artist.name if album.artist else 'Unknown',
        'artist_id': album.artist.id if album.artist else None,
        'num_tracks': album.num_tracks,
        'num_volumes': getattr(album, 'num_volumes', 1),
        'duration': album.duration,
        'year': album.year,
        'release_date': str(album.release_date) if album.release_date else '',
        'audio_quality': getattr(album, 'audio_quality', ''),
        'explicit': getattr(album, 'explicit', False),
        'image_url': album.image(640) if album else '',
        'image_url_small': album.image(160) if album else '',
        'image_url_large': album.image(1280) if album else '',
    }
```

---

## Phase 6: Quality & Polish

### 6.1 Audio Quality Badge

Show quality in Now Playing view as a pill:
- `LOW` → "320k" badge, muted
- `LOSSLESS` → "LOSSLESS" badge, primary color
- `HI_RES_LOSSLESS` → "HI-RES" badge, accent color
- Dolby Atmos → "ATMOS" badge

### 6.2 Explicit Content Badge

Small "E" badge next to track titles that have `explicit: true`.

### 6.3 Now-Playing Indicator in Track Lists

When a track in a list is the currently playing track, show:
- Animated equalizer bars (3 small rectangles pulsing) instead of track number
- Primary color highlight on the row

### 6.4 Album Art in MiniPlayer

Replace the music icon placeholder with actual album art Image when a URL is available (both Tidal and Bluetooth sources).

### 6.5 Smooth Transitions

- Browse → Now Playing: 300ms slide from right
- Now Playing → Queue: 200ms slide from right
- Track change: 150ms crossfade on album art
- All using Easing.OutCubic

---

## Implementation Order

### Sprint 1: Playback (make it play music)
1. Add GStreamer to CMakeLists.txt
2. Add GStreamer playbin to TidalClient (play/pause/stop/seek/position/duration)
3. Add queue management (playTrackInContext, next, previous, shuffle, repeat)
4. Wire `streamReady` → GStreamer playbin
5. Test: tap a track in current Tidal.qml → hear audio

### Sprint 2: Now Playing UI
1. Redesign Tidal.qml with view states (browse/nowPlaying/queue)
2. Build Now Playing view (album art, metadata, progress bar, controls)
3. Build queue panel (slide-in from right)
4. Wire all controls to TidalClient slots

### Sprint 3: MiniPlayer + Source Management
1. Refactor MiniPlayer.qml for multi-source support
2. Wire activeAudioSource in Main.qml
3. Add album art Image to MiniPlayer (both sources)
4. Add source badge to MiniPlayer
5. Test: play Tidal → navigate to Maps → MiniPlayer shows Tidal track

### Sprint 4: Browse Polish
1. Redesign track list rows (76px height, larger art, quality badges)
2. Build album detail view (header + track list)
3. Build artist detail view (photo + top tracks + album grid)
4. Build home view (mixes + recent favorites)
5. Enhance search with album/artist result cards

### Sprint 5: Service Enhancements
1. Add favorite/unfavorite commands to tidal_service.py
2. Enhanced metadata (explicit, quality, version, multiple artists)
3. Mix images and enhanced home page data
4. Error handling and edge cases (network loss, token expiry)

### Sprint 6: Final Polish
1. Now-playing indicator in track lists
2. Explicit badges
3. Quality badges in Now Playing
4. Transition animations
5. Test all 3 themes (Cyberpunk, RetroVFD, Monochrome)
6. Test with real Tidal account end-to-end
