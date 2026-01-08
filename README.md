# oscyl

A minimal audio player for Linux with a terminal-inspired graphical interface. Plays FLAC and Ogg Vorbis files.

## Features

- FLAC and Ogg Vorbis playback
- Directory-based playlists with alphabetical sorting
- Shuffle and repeat modes (off, one, all)
- Seeking and volume control
- Progress bar with elapsed/total time display
- Directory browser for navigating to different folders
- Auto-advance to next track
- Keyboard-driven interface

## Screenshot

```
+-------------------------------------+
|  Now Playing: Artist - Track        |
|  [>]  01:23 / 03:45    [S][-] 100%  |
|  ================================   |
+-------------------------------------+
|  01. Track One                      |
|  02. Track Two            < current |
|  03. Track Three                    |
|  04. Track Four                     |
+-------------------------------------+
```

## Dependencies

- libFLAC
- libvorbisfile
- raylib

On Debian/Ubuntu:

```bash
sudo apt install build-essential libflac-dev libvorbis-dev
```

raylib must be built from source. See: https://github.com/raysan5/raylib/wiki/Working-on-GNU-Linux

## Building

```bash
make        # builds ./oscyl
make clean  # removes build artifacts
```

## Usage

```bash
./oscyl /path/to/music/directory
```

## Controls

| Key | Action |
|-----|--------|
| Up/Down | Navigate track list |
| Enter | Play selected track |
| Space | Pause/resume |
| Left/Right | Seek -/+ 10 seconds |
| +/- | Adjust volume |
| S | Toggle shuffle |
| R | Cycle repeat mode (off/one/all) |
| Tab | Open/close directory browser |
| Esc | Close directory browser |
| Q | Quit |

## Tech Stack

- C (C99)
- miniaudio for audio output (bundled)
- libFLAC for FLAC decoding
- libvorbisfile for Ogg Vorbis decoding
- raylib for graphics

## License

This project is released into the public domain under the Unlicense. See UNLICENSE for details.
