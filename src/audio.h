#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>

typedef enum {
    AUDIO_FORMAT_UNKNOWN,
    AUDIO_FORMAT_FLAC,
    AUDIO_FORMAT_VORBIS
} AudioFormat;

typedef enum {
    AUDIO_STATE_STOPPED,
    AUDIO_STATE_PLAYING,
    AUDIO_STATE_PAUSED
} AudioState;

// Initialize the audio system. Call once at startup.
bool audio_init(void);

// Shutdown the audio system. Call once at exit.
void audio_shutdown(void);

// Load and start playing a file. Returns false on error.
bool audio_play_file(const char *path);

// Stop playback and unload current file.
void audio_stop(void);

// Toggle between playing and paused.
void audio_toggle_pause(void);

// Get current playback state.
AudioState audio_get_state(void);

// Check if playback has finished (end of file reached).
bool audio_is_finished(void);

// Get current position in seconds.
double audio_get_position(void);

// Get total duration in seconds.
double audio_get_duration(void);

// Seek to a position in seconds. Returns false if seeking not supported or failed.
bool audio_seek(double position);

// Set volume (0.0 to 1.0).
void audio_set_volume(float volume);

// Get current volume (0.0 to 1.0).
float audio_get_volume(void);

#endif
