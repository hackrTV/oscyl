# oscyl: A TUI-Style Audio Player — Project Spec

## Overview

A minimal audio player for Linux that plays FLAC and Ogg Vorbis files. The UI has a flat, terminal-inspired aesthetic (think TuiCSS but with a darker, more muted palette). This is a graphical application that *looks* like a TUI, not an actual terminal app.

## Goals

1. Play FLAC and Ogg Vorbis audio files
2. Build a custom UI with a specific visual aesthetic
3. Learn about audio programming and low-level GUI rendering
4. Keep dependencies minimal

## Non-Goals (Scope Boundaries)

- No plugin system
- No streaming/network audio
- No fancy visualizers
- No editable playlists (just directory-based playback for now)
- No configuration files in Phase 1-3
- No custom widget framework — keep UI elements hardcoded and simple

## Technology Stack

| Component | Choice | Rationale |
|-----------|--------|-----------|
| Language | C | Direct access to all libraries, no runtime, maximum learning |
| Audio output | miniaudio | Single-header, handles ALSA/PipeWire/Pulse automatically |
| FLAC decoding | libFLAC | Standard, stable, well-documented |
| Vorbis decoding | libvorbisfile | Standard, stable, wraps libvorbis nicely |
| Graphics | raylib | Simple API, minimal deps, easy text/shape rendering |
| Build | make | Simple, no external build tool dependencies |

### Alternative: Use miniaudio's built-in decoders

miniaudio has optional built-in FLAC and Vorbis decoding (via stb_vorbis and dr_flac). This reduces dependencies further. Consider this if you want maximum simplicity — just define `MA_NO_DECODING` to disable, or enable the built-ins and skip libFLAC/libvorbis entirely.

## Visual Design

**Aesthetic:** Flat rectangles, monospace bitmap font, box-drawing characters optional. Inspired by TuiCSS but darker and more muted.

**Suggested palette (adjust to taste):**
```
Background:     #1a1a1a
Panel bg:       #242424
Border:         #3a3a3a
Text primary:   #a0a0a0
Text secondary: #606060
Accent:         #5f8787 (muted teal)
Accent alt:     #875f5f (muted red)
```

**Font:** Use a bitmap font (e.g., Terminus, Cozette, or render your own from a PNG atlas). Avoid TTF complexity initially.

**Layout (simple starting point):**
```
┌─────────────────────────────────────┐
│  Now Playing: Artist - Track        │
│  ──────────────────────────────     │
│  [▶]  00:00 / 03:45                 │
├─────────────────────────────────────┤
│  01. Track One                      │
│  02. Track Two            ◄ current │
│  03. Track Three                    │
│  04. Track Four                     │
└─────────────────────────────────────┘
```

## Phased Implementation

### Phase 1: Headless Audio Playback

**Deliverable:** CLI program that plays a single audio file passed as argument.

**Tasks:**
1. Set up project structure and Makefile
2. Initialize miniaudio device
3. Decode FLAC file and feed samples to miniaudio callback
4. Add Vorbis support
5. Handle end-of-file gracefully
6. Add play/pause via spacebar (terminal raw mode or simple stdin polling)

**Success criteria:** `./player song.flac` plays audio, spacebar pauses, Ctrl+C exits.

**Do not add:** GUI, playlists, seeking, volume control.

---

### Phase 2: Minimal Window

**Deliverable:** A raylib window that shows the current track name while audio plays.

**Tasks:**
1. Add raylib to build
2. Open a fixed-size window (e.g., 600x400)
3. Load a bitmap font
4. Display track filename as text
5. Move audio to a separate thread (raylib wants the main thread)
6. Spacebar still toggles play/pause

**Success criteria:** Window opens, shows track name, audio plays, spacebar pauses.

**Do not add:** Progress bar, file browser, multiple tracks, fancy layout.

---

### Phase 3: Basic UI

**Deliverable:** Panel layout with now-playing area and track list.

**Tasks:**
1. Draw rectangles for panels using the color palette
2. Render box-drawing characters or simple lines for borders
3. Show current track in "now playing" panel
4. Show hardcoded list of tracks (just filenames from a directory)
5. Highlight current track in list
6. Up/down arrows to select, Enter to play selected

**Success criteria:** Looks like the layout mockup. Can navigate and play different tracks.

**Do not add:** Seeking, volume, album art, animated transitions.

---

### Phase 4: Playback Features

**Deliverable:** Seeking, progress bar, volume control.

**Tasks:**
1. Display elapsed / total time
2. Draw progress bar (filled rectangle proportional to position)
3. Left/right arrows to seek ±10 seconds
4. Up/down (or +/-) for volume
5. Show volume indicator

**Success criteria:** Full transport controls work.

---

### Phase 5: Polish (Optional)

Ideas for later, only if the core is solid:

- Album art display (raylib can load images easily)
- Directory browser / file picker
- Shuffle / repeat modes
- Config file for colors/keybindings
- Remember last position on exit

## Project Structure

```
player/
├── Makefile
├── src/
│   ├── main.c          # Entry point, raylib main loop
│   ├── audio.c/.h      # Playback engine (miniaudio + decoders)
│   ├── ui.c/.h         # Drawing functions
│   └── playlist.c/.h   # Track list management
├── assets/
│   └── font.png        # Bitmap font atlas
└── SPEC.md             # This file
```

## Build Dependencies (Debian/Ubuntu)

```bash
# Core
sudo apt install build-essential libflac-dev libvorbis-dev

# raylib (build from source or use package if recent enough)
# See: https://github.com/raysan5/raylib/wiki/Working-on-GNU-Linux
```

## Reference Resources

- miniaudio: https://miniaud.io/docs/
- raylib: https://www.raylib.com/cheatsheet/cheatsheet.html
- libFLAC: https://xiph.org/flac/api/
- libvorbisfile: https://xiph.org/vorbis/doc/vorbisfile/

## Anti-Yak-Shaving Rules

1. **No config files until Phase 5.** Hardcode everything.
2. **No abstractions "for later."** Write the specific code you need now.
3. **If a feature isn't in the current phase, don't build scaffolding for it.**
4. **One file format for fonts.** Pick PNG atlas and stick with it.
5. **Fixed window size.** No resize handling until it matters.
6. **If you're writing a "utility function," stop and ask if you actually need it.**
