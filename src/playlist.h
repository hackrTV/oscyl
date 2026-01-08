#ifndef PLAYLIST_H
#define PLAYLIST_H

#include <stdbool.h>

#define PLAYLIST_MAX_TRACKS 256
#define PLAYLIST_MAX_PATH 512

typedef enum {
    REPEAT_OFF,
    REPEAT_ONE,
    REPEAT_ALL
} RepeatMode;

typedef struct {
    char paths[PLAYLIST_MAX_TRACKS][PLAYLIST_MAX_PATH];
    char names[PLAYLIST_MAX_TRACKS][PLAYLIST_MAX_PATH];  // filename only
    char dir_path[PLAYLIST_MAX_PATH];  // current directory
    int count;
    int current;    // currently playing track (-1 if none)
    int selected;   // cursor position for navigation

    // Playback modes
    bool shuffle;
    RepeatMode repeat;
    int shuffle_order[PLAYLIST_MAX_TRACKS];  // shuffled indices
    int shuffle_pos;  // position in shuffle order
} Playlist;

// Scan a directory for .flac and .ogg files. Returns false if directory can't be opened.
bool playlist_scan(Playlist *pl, const char *dir_path);

// Get path of currently selected track
const char *playlist_selected_path(const Playlist *pl);

// Get name of currently selected track
const char *playlist_selected_name(const Playlist *pl);

// Get name of currently playing track (or NULL if none)
const char *playlist_current_name(const Playlist *pl);

// Navigation
void playlist_select_next(Playlist *pl);
void playlist_select_prev(Playlist *pl);

// Set current playing track to selected
void playlist_play_selected(Playlist *pl);

// Get the next track index to play (-1 if none). Does not change state.
int playlist_next_track(Playlist *pl);

// Advance to next track. Returns the new track index (-1 if end of playlist).
int playlist_advance(Playlist *pl);

// Toggle shuffle mode
void playlist_toggle_shuffle(Playlist *pl);

// Cycle repeat mode (off -> one -> all -> off)
void playlist_cycle_repeat(Playlist *pl);

// Get current directory path
const char *playlist_get_dir(const Playlist *pl);

#endif
