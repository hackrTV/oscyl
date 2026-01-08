#define _DEFAULT_SOURCE

#include "audio.h"
#include "playlist.h"

#include <raylib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdlib.h>

#define WINDOW_WIDTH 600
#define WINDOW_HEIGHT 400
#define FONT_SIZE 16
#define LINE_HEIGHT 20
#define PANEL_PADDING 10
#define BORDER_WIDTH 1

// Colors from spec
static const Color COLOR_BG         = { 0x1a, 0x1a, 0x1a, 0xff };
static const Color COLOR_PANEL      = { 0x24, 0x24, 0x24, 0xff };
static const Color COLOR_BORDER     = { 0x3a, 0x3a, 0x3a, 0xff };
static const Color COLOR_TEXT       = { 0xa0, 0xa0, 0xa0, 0xff };
static const Color COLOR_TEXT_DIM   = { 0x60, 0x60, 0x60, 0xff };
static const Color COLOR_ACCENT     = { 0x5f, 0x87, 0x87, 0xff };

// Layout
#define NOW_PLAYING_HEIGHT 80
#define TRACK_LIST_Y (NOW_PLAYING_HEIGHT)
#define TRACK_LIST_HEIGHT (WINDOW_HEIGHT - NOW_PLAYING_HEIGHT)
#define MAX_VISIBLE_TRACKS ((TRACK_LIST_HEIGHT - PANEL_PADDING * 2) / LINE_HEIGHT)

static void draw_panel(int x, int y, int w, int h) {
    DrawRectangle(x, y, w, h, COLOR_PANEL);
    DrawRectangleLines(x, y, w, h, COLOR_BORDER);
}

static void format_time(double seconds, char *buf, size_t size) {
    int mins = (int)(seconds / 60);
    int secs = (int)(seconds) % 60;
    snprintf(buf, size, "%02d:%02d", mins, secs);
}

static const char *state_icon(AudioState state) {
    switch (state) {
        case AUDIO_STATE_PLAYING: return "[>]";
        case AUDIO_STATE_PAUSED:  return "[||]";
        default:                  return "[.]";
    }
}

// Directory browser
#define BROWSER_MAX_ENTRIES 256

typedef struct {
    bool active;
    char path[PLAYLIST_MAX_PATH];
    char entries[BROWSER_MAX_ENTRIES][256];
    bool is_dir[BROWSER_MAX_ENTRIES];
    int count;
    int selected;
    int scroll_offset;
} Browser;

static int compare_entries(const void *a, const void *b) {
    return strcasecmp((const char *)a, (const char *)b);
}

static void browser_scan(Browser *br, const char *path) {
    br->count = 0;
    br->selected = 0;
    br->scroll_offset = 0;
    strncpy(br->path, path, PLAYLIST_MAX_PATH - 1);
    br->path[PLAYLIST_MAX_PATH - 1] = '\0';

    // Remove trailing slash if present (unless root)
    size_t len = strlen(br->path);
    if (len > 1 && br->path[len - 1] == '/') {
        br->path[len - 1] = '\0';
    }

    DIR *dir = opendir(br->path);
    if (!dir) return;

    // Add parent directory entry if not at root
    if (strcmp(br->path, "/") != 0) {
        strcpy(br->entries[br->count], "..");
        br->is_dir[br->count] = true;
        br->count++;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && br->count < BROWSER_MAX_ENTRIES) {
        // Skip hidden files and . / ..
        if (entry->d_name[0] == '.') continue;

        // Check if it's a directory
        char full_path[PLAYLIST_MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", br->path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            strncpy(br->entries[br->count], entry->d_name, 255);
            br->entries[br->count][255] = '\0';
            br->is_dir[br->count] = true;
            br->count++;
        }
    }

    closedir(dir);

    // Sort entries (skip ".." at index 0 if present)
    int start = (strcmp(br->path, "/") != 0) ? 1 : 0;
    if (br->count > start + 1) {
        qsort(&br->entries[start], br->count - start, 256, compare_entries);
    }
}

static void browser_select_entry(Browser *br, Playlist *pl) {
    if (br->count == 0) return;

    const char *selected = br->entries[br->selected];

    if (strcmp(selected, "..") == 0) {
        // Go to parent directory
        char *last_slash = strrchr(br->path, '/');
        if (last_slash && last_slash != br->path) {
            *last_slash = '\0';
        } else if (last_slash == br->path) {
            br->path[1] = '\0';  // Root
        }
        browser_scan(br, br->path);
    } else {
        // Enter subdirectory
        char new_path[PLAYLIST_MAX_PATH];
        if (strcmp(br->path, "/") == 0) {
            snprintf(new_path, sizeof(new_path), "/%s", selected);
        } else {
            snprintf(new_path, sizeof(new_path), "%s/%s", br->path, selected);
        }

        // Check if this directory has audio files
        Playlist test_pl = {0};
        if (playlist_scan(&test_pl, new_path) && test_pl.count > 0) {
            // Load this directory into the main playlist
            audio_stop();
            playlist_scan(pl, new_path);
            br->active = false;
        } else {
            // Just navigate into it
            browser_scan(br, new_path);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <directory>\n", argv[0]);
        return 1;
    }

    const char *dir_path = argv[1];

    // Initialize audio
    if (!audio_init()) {
        fprintf(stderr, "Failed to initialize audio\n");
        return 1;
    }

    // Scan directory for tracks
    Playlist playlist = {0};
    if (!playlist_scan(&playlist, dir_path)) {
        fprintf(stderr, "Failed to scan directory: %s\n", dir_path);
        audio_shutdown();
        return 1;
    }

    if (playlist.count == 0) {
        fprintf(stderr, "No audio files found in: %s\n", dir_path);
        audio_shutdown();
        return 1;
    }

    // Initialize raylib window
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "oscyl");
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);  // Disable Esc closing window

    // Load font
    Font font = LoadFontEx("assets/terminus.ttf", FONT_SIZE, NULL, 0);
    if (font.texture.id == 0) {
        fprintf(stderr, "Warning: Failed to load font, using default\n");
        font = GetFontDefault();
    }

    int scroll_offset = 0;

    // Browser state
    Browser browser = {0};

    // Main loop
    while (!WindowShouldClose()) {
        // Input: toggle browser
        if (IsKeyPressed(KEY_TAB)) {
            browser.active = !browser.active;
            if (browser.active) {
                // Start browsing from current playlist directory
                const char *dir = playlist_get_dir(&playlist);
                if (dir && dir[0]) {
                    browser_scan(&browser, dir);
                } else {
                    browser_scan(&browser, "/");
                }
            }
        }

        // Input: quit (always available)
        if (IsKeyPressed(KEY_Q)) {
            break;
        }

        if (browser.active) {
            // Browser navigation
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressedRepeat(KEY_DOWN)) {
                if (browser.count > 0 && browser.selected < browser.count - 1) {
                    browser.selected++;
                    int visible = MAX_VISIBLE_TRACKS - 1;
                    if (browser.selected >= browser.scroll_offset + visible) {
                        browser.scroll_offset = browser.selected - visible + 1;
                    }
                }
            }
            if (IsKeyPressed(KEY_UP) || IsKeyPressedRepeat(KEY_UP)) {
                if (browser.selected > 0) {
                    browser.selected--;
                    if (browser.selected < browser.scroll_offset) {
                        browser.scroll_offset = browser.selected;
                    }
                }
            }
            if (IsKeyPressed(KEY_ENTER)) {
                browser_select_entry(&browser, &playlist);
                scroll_offset = 0;  // Reset playlist scroll when loading new dir
            }
            if (IsKeyPressed(KEY_ESCAPE)) {
                browser.active = false;
            }
        } else {
            // Playlist navigation
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressedRepeat(KEY_DOWN)) {
                playlist_select_next(&playlist);
                if (playlist.selected >= scroll_offset + (int)MAX_VISIBLE_TRACKS) {
                    scroll_offset = playlist.selected - (int)MAX_VISIBLE_TRACKS + 1;
                }
                if (scroll_offset < 0) scroll_offset = 0;
            }
            if (IsKeyPressed(KEY_UP) || IsKeyPressedRepeat(KEY_UP)) {
                playlist_select_prev(&playlist);
                if (playlist.selected < scroll_offset) {
                    scroll_offset = playlist.selected;
                }
                if (scroll_offset < 0) scroll_offset = 0;
            }

            // Input: play selected
            if (IsKeyPressed(KEY_ENTER)) {
                playlist_play_selected(&playlist);
                const char *path = playlist_selected_path(&playlist);
                if (path) {
                    audio_stop();
                    audio_play_file(path);
                }
            }

            // Input: pause/resume
            if (IsKeyPressed(KEY_SPACE)) {
                audio_toggle_pause();
            }

            // Input: seek
            if (IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT)) {
                double pos = audio_get_position();
                audio_seek(pos - 10.0);
            }
            if (IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT)) {
                double pos = audio_get_position();
                audio_seek(pos + 10.0);
            }

            // Input: volume
            if (IsKeyPressed(KEY_EQUAL) || IsKeyPressedRepeat(KEY_EQUAL) ||
                IsKeyPressed(KEY_KP_ADD) || IsKeyPressedRepeat(KEY_KP_ADD)) {
                float vol = audio_get_volume();
                audio_set_volume(vol + 0.05f);
            }
            if (IsKeyPressed(KEY_MINUS) || IsKeyPressedRepeat(KEY_MINUS) ||
                IsKeyPressed(KEY_KP_SUBTRACT) || IsKeyPressedRepeat(KEY_KP_SUBTRACT)) {
                float vol = audio_get_volume();
                audio_set_volume(vol - 0.05f);
            }

            // Input: shuffle/repeat
            if (IsKeyPressed(KEY_S)) {
                playlist_toggle_shuffle(&playlist);
            }
            if (IsKeyPressed(KEY_R)) {
                playlist_cycle_repeat(&playlist);
            }
        }

        // Auto-advance when track finishes
        if (audio_is_finished() && playlist.current >= 0) {
            int next = playlist_advance(&playlist);
            if (next >= 0) {
                audio_play_file(playlist.paths[next]);
                // Scroll if needed
                if (playlist.selected >= scroll_offset + MAX_VISIBLE_TRACKS) {
                    scroll_offset = playlist.selected - MAX_VISIBLE_TRACKS + 1;
                } else if (playlist.selected < scroll_offset) {
                    scroll_offset = playlist.selected;
                }
            } else {
                // End of playlist
                audio_stop();
                playlist.current = -1;
            }
        }

        // Draw
        BeginDrawing();
        ClearBackground(COLOR_BG);

        // Now Playing panel
        draw_panel(0, 0, WINDOW_WIDTH, NOW_PLAYING_HEIGHT);

        const char *current_name = playlist_current_name(&playlist);
        Vector2 pos = { PANEL_PADDING, PANEL_PADDING };

        if (current_name) {
            char now_playing[256];
            snprintf(now_playing, sizeof(now_playing), "Now Playing: %s", current_name);
            DrawTextEx(font, now_playing, pos, FONT_SIZE, 1, COLOR_TEXT);
        } else {
            DrawTextEx(font, "Now Playing: -", pos, FONT_SIZE, 1, COLOR_TEXT_DIM);
        }

        // Playback state icon and time
        pos.y += LINE_HEIGHT + 8;
        const char *icon = state_icon(audio_get_state());
        DrawTextEx(font, icon, pos, FONT_SIZE, 1, COLOR_ACCENT);

        // Time display
        double position = audio_get_position();
        double duration = audio_get_duration();
        char pos_str[16], dur_str[16], time_str[48];
        format_time(position, pos_str, sizeof(pos_str));
        format_time(duration, dur_str, sizeof(dur_str));
        snprintf(time_str, sizeof(time_str), "%s / %s", pos_str, dur_str);
        Vector2 time_pos = { pos.x + 50, pos.y };
        DrawTextEx(font, time_str, time_pos, FONT_SIZE, 1, COLOR_TEXT);

        // Shuffle/Repeat/Volume display
        char mode_str[32];
        const char *repeat_str = playlist.repeat == REPEAT_ONE ? "1" :
                                 playlist.repeat == REPEAT_ALL ? "A" : "-";
        snprintf(mode_str, sizeof(mode_str), "[%s][%s] %d%%",
                 playlist.shuffle ? "S" : "-",
                 repeat_str,
                 (int)(audio_get_volume() * 100));
        Vector2 mode_pos = { WINDOW_WIDTH - 110, pos.y };
        DrawTextEx(font, mode_str, mode_pos, FONT_SIZE, 1, COLOR_TEXT_DIM);

        // Progress bar
        pos.y += LINE_HEIGHT + 4;
        int bar_x = PANEL_PADDING;
        int bar_y = (int)pos.y;
        int bar_w = WINDOW_WIDTH - PANEL_PADDING * 2;
        int bar_h = 6;
        DrawRectangle(bar_x, bar_y, bar_w, bar_h, COLOR_BORDER);
        if (duration > 0) {
            int fill_w = (int)(bar_w * (position / duration));
            if (fill_w > bar_w) fill_w = bar_w;
            DrawRectangle(bar_x, bar_y, fill_w, bar_h, COLOR_ACCENT);
        }

        // Track list / Browser panel
        draw_panel(0, TRACK_LIST_Y, WINDOW_WIDTH, TRACK_LIST_HEIGHT);

        pos.x = PANEL_PADDING;
        pos.y = TRACK_LIST_Y + PANEL_PADDING;

        if (browser.active) {
            // Draw browser header
            char header[280];
            snprintf(header, sizeof(header), "Browse: %s", browser.path);
            DrawTextEx(font, header, pos, FONT_SIZE, 1, COLOR_ACCENT);
            pos.y += LINE_HEIGHT;

            // Draw directory entries
            int visible = MAX_VISIBLE_TRACKS - 1;  // -1 for header
            for (int i = 0; i < visible && (browser.scroll_offset + i) < browser.count; i++) {
                int idx = browser.scroll_offset + i;
                char line[280];
                snprintf(line, sizeof(line), "  [DIR] %s", browser.entries[idx]);

                Color color = COLOR_TEXT_DIM;
                if (idx == browser.selected) {
                    DrawRectangle(PANEL_PADDING - 2, (int)pos.y - 2,
                                  WINDOW_WIDTH - PANEL_PADDING * 2 + 4, LINE_HEIGHT,
                                  COLOR_BORDER);
                    color = COLOR_TEXT;
                }

                DrawTextEx(font, line, pos, FONT_SIZE, 1, color);
                pos.y += LINE_HEIGHT;
            }

            // Browser hint
            Vector2 hint_pos = { PANEL_PADDING, WINDOW_HEIGHT - LINE_HEIGHT - 5 };
            DrawTextEx(font, "Tab:close  Enter:select  Esc:cancel", hint_pos, FONT_SIZE, 1, COLOR_TEXT_DIM);
        } else {
            // Draw track list
            for (int i = 0; i < MAX_VISIBLE_TRACKS && (scroll_offset + i) < playlist.count; i++) {
                int track_idx = scroll_offset + i;
                char line[280];
                snprintf(line, sizeof(line), "%2d. %s", track_idx + 1, playlist.names[track_idx]);

                Color color = COLOR_TEXT_DIM;
                if (track_idx == playlist.current) {
                    color = COLOR_ACCENT;
                }
                if (track_idx == playlist.selected) {
                    DrawRectangle(PANEL_PADDING - 2, (int)pos.y - 2,
                                  WINDOW_WIDTH - PANEL_PADDING * 2 + 4, LINE_HEIGHT,
                                  COLOR_BORDER);
                    color = COLOR_TEXT;
                }

                DrawTextEx(font, line, pos, FONT_SIZE, 1, color);
                pos.y += LINE_HEIGHT;
            }

            // Scroll indicator if needed
            if (playlist.count > MAX_VISIBLE_TRACKS) {
                char scroll_info[32];
                snprintf(scroll_info, sizeof(scroll_info), "[%d-%d of %d]",
                         scroll_offset + 1,
                         scroll_offset + MAX_VISIBLE_TRACKS > playlist.count ?
                             playlist.count : scroll_offset + (int)MAX_VISIBLE_TRACKS,
                         playlist.count);
                Vector2 scroll_pos = { WINDOW_WIDTH - 100, WINDOW_HEIGHT - LINE_HEIGHT - 5 };
                DrawTextEx(font, scroll_info, scroll_pos, FONT_SIZE, 1, COLOR_TEXT_DIM);
            }
        }

        EndDrawing();
    }

    // Cleanup
    UnloadFont(font);
    CloseWindow();
    audio_shutdown();

    return 0;
}
