#define _DEFAULT_SOURCE

#include "playlist.h"

#include <dirent.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static bool seeded = false;

static bool is_audio_file(const char *name) {
    const char *ext = strrchr(name, '.');
    if (!ext) return false;
    return strcasecmp(ext, ".flac") == 0 || strcasecmp(ext, ".ogg") == 0;
}

static void generate_shuffle_order(Playlist *pl) {
    if (!seeded) {
        srand((unsigned int)time(NULL));
        seeded = true;
    }

    // Initialize with sequential order
    for (int i = 0; i < pl->count; i++) {
        pl->shuffle_order[i] = i;
    }

    // Fisher-Yates shuffle
    for (int i = pl->count - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int tmp = pl->shuffle_order[i];
        pl->shuffle_order[i] = pl->shuffle_order[j];
        pl->shuffle_order[j] = tmp;
    }

    pl->shuffle_pos = 0;
}

bool playlist_scan(Playlist *pl, const char *dir_path) {
    // Preserve playback modes across rescans
    bool was_shuffle = pl->shuffle;
    RepeatMode was_repeat = pl->repeat;

    pl->count = 0;
    pl->current = -1;
    pl->selected = 0;
    pl->shuffle = was_shuffle;
    pl->repeat = was_repeat;
    pl->shuffle_pos = 0;

    strncpy(pl->dir_path, dir_path, PLAYLIST_MAX_PATH - 1);
    pl->dir_path[PLAYLIST_MAX_PATH - 1] = '\0';

    DIR *dir = opendir(dir_path);
    if (!dir) {
        return false;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && pl->count < PLAYLIST_MAX_TRACKS) {
        if (entry->d_type != DT_REG && entry->d_type != DT_UNKNOWN) {
            continue;  // skip non-files
        }

        if (!is_audio_file(entry->d_name)) {
            continue;
        }

        // Store filename
        strncpy(pl->names[pl->count], entry->d_name, PLAYLIST_MAX_PATH - 1);
        pl->names[pl->count][PLAYLIST_MAX_PATH - 1] = '\0';

        // Store full path
        snprintf(pl->paths[pl->count], PLAYLIST_MAX_PATH, "%s/%s", dir_path, entry->d_name);

        pl->count++;
    }

    closedir(dir);

    // Sort by filename first
    if (pl->count > 1) {
        // Sort both arrays together by sorting names and keeping paths in sync
        // Simple bubble sort since we're dealing with small lists
        for (int i = 0; i < pl->count - 1; i++) {
            for (int j = 0; j < pl->count - i - 1; j++) {
                if (strcasecmp(pl->names[j], pl->names[j + 1]) > 0) {
                    // Swap names
                    char temp[PLAYLIST_MAX_PATH];
                    strcpy(temp, pl->names[j]);
                    strcpy(pl->names[j], pl->names[j + 1]);
                    strcpy(pl->names[j + 1], temp);
                    // Swap paths
                    strcpy(temp, pl->paths[j]);
                    strcpy(pl->paths[j], pl->paths[j + 1]);
                    strcpy(pl->paths[j + 1], temp);
                }
            }
        }
    }

    // Generate shuffle order
    generate_shuffle_order(pl);

    return true;
}

const char *playlist_selected_path(const Playlist *pl) {
    if (pl->count == 0 || pl->selected < 0 || pl->selected >= pl->count) {
        return NULL;
    }
    return pl->paths[pl->selected];
}

const char *playlist_selected_name(const Playlist *pl) {
    if (pl->count == 0 || pl->selected < 0 || pl->selected >= pl->count) {
        return NULL;
    }
    return pl->names[pl->selected];
}

const char *playlist_current_name(const Playlist *pl) {
    if (pl->current < 0 || pl->current >= pl->count) {
        return NULL;
    }
    return pl->names[pl->current];
}

void playlist_select_next(Playlist *pl) {
    if (pl->count == 0) return;
    pl->selected++;
    if (pl->selected >= pl->count) {
        pl->selected = pl->count - 1;
    }
}

void playlist_select_prev(Playlist *pl) {
    if (pl->count == 0) return;
    pl->selected--;
    if (pl->selected < 0) {
        pl->selected = 0;
    }
}

void playlist_play_selected(Playlist *pl) {
    if (pl->count == 0) return;
    pl->current = pl->selected;

    // Sync shuffle position to current track
    if (pl->shuffle) {
        for (int i = 0; i < pl->count; i++) {
            if (pl->shuffle_order[i] == pl->current) {
                pl->shuffle_pos = i;
                break;
            }
        }
    }
}

int playlist_next_track(Playlist *pl) {
    if (pl->count == 0 || pl->current < 0) return -1;

    // Repeat one: return same track
    if (pl->repeat == REPEAT_ONE) {
        return pl->current;
    }

    int next;
    if (pl->shuffle) {
        int next_pos = pl->shuffle_pos + 1;
        if (next_pos >= pl->count) {
            if (pl->repeat == REPEAT_ALL) {
                next_pos = 0;
            } else {
                return -1;  // End of shuffled playlist
            }
        }
        // Bounds check
        if (next_pos < 0 || next_pos >= pl->count) return -1;
        next = pl->shuffle_order[next_pos];
        // Validate the index
        if (next < 0 || next >= pl->count) return -1;
    } else {
        next = pl->current + 1;
        if (next >= pl->count) {
            if (pl->repeat == REPEAT_ALL) {
                next = 0;
            } else {
                return -1;  // End of playlist
            }
        }
    }

    return next;
}

int playlist_advance(Playlist *pl) {
    int next = playlist_next_track(pl);
    if (next >= 0) {
        pl->current = next;
        pl->selected = next;
        if (pl->shuffle) {
            pl->shuffle_pos++;
            if (pl->shuffle_pos >= pl->count) {
                pl->shuffle_pos = 0;
            }
        }
    }
    return next;
}

void playlist_toggle_shuffle(Playlist *pl) {
    pl->shuffle = !pl->shuffle;
    if (pl->shuffle) {
        generate_shuffle_order(pl);
        // Sync shuffle position to current track
        if (pl->current >= 0) {
            for (int i = 0; i < pl->count; i++) {
                if (pl->shuffle_order[i] == pl->current) {
                    pl->shuffle_pos = i;
                    break;
                }
            }
        }
    }
}

void playlist_cycle_repeat(Playlist *pl) {
    switch (pl->repeat) {
        case REPEAT_OFF: pl->repeat = REPEAT_ONE; break;
        case REPEAT_ONE: pl->repeat = REPEAT_ALL; break;
        case REPEAT_ALL: pl->repeat = REPEAT_OFF; break;
    }
}

const char *playlist_get_dir(const Playlist *pl) {
    return pl->dir_path;
}
