#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "audio.h"

#include <FLAC/stream_decoder.h>
#include <vorbis/vorbisfile.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AUDIO_BUFFER_SIZE (16384)

typedef struct {
    // Miniaudio
    ma_device device;
    bool device_initialized;

    // Current format
    AudioFormat format;
    AudioState state;
    bool finished;

    // Audio properties
    unsigned int sample_rate;
    unsigned int channels;
    uint64_t total_samples;    // total samples in file
    uint64_t samples_played;   // samples played so far

    // Volume (0.0 to 1.0)
    float volume;

    // FLAC decoder
    FLAC__StreamDecoder *flac_decoder;
    FILE *flac_file;

    // Vorbis decoder
    OggVorbis_File vorbis_file;
    bool vorbis_open;

    // Sample buffer (interleaved float samples)
    float buffer[AUDIO_BUFFER_SIZE];
    size_t buffer_read_pos;
    size_t buffer_write_pos;
    size_t buffer_count;
} AudioContext;

static AudioContext ctx = {0};

// Forward declarations
static void audio_callback(ma_device *device, void *output, const void *input, ma_uint32 frame_count);
static bool decode_flac_samples(void);
static bool decode_vorbis_samples(void);

// FLAC callbacks
static FLAC__StreamDecoderWriteStatus flac_write_callback(
    const FLAC__StreamDecoder *decoder,
    const FLAC__Frame *frame,
    const FLAC__int32 *const buffer[],
    void *client_data);
static void flac_metadata_callback(
    const FLAC__StreamDecoder *decoder,
    const FLAC__StreamMetadata *metadata,
    void *client_data);
static void flac_error_callback(
    const FLAC__StreamDecoder *decoder,
    FLAC__StreamDecoderErrorStatus status,
    void *client_data);

bool audio_init(void) {
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate = 44100;
    config.dataCallback = audio_callback;
    config.pUserData = &ctx;

    if (ma_device_init(NULL, &config, &ctx.device) != MA_SUCCESS) {
        fprintf(stderr, "Failed to initialize audio device\n");
        return false;
    }

    ctx.device_initialized = true;
    ctx.state = AUDIO_STATE_STOPPED;
    ctx.volume = 1.0f;
    return true;
}

void audio_shutdown(void) {
    audio_stop();

    if (ctx.device_initialized) {
        ma_device_uninit(&ctx.device);
        ctx.device_initialized = false;
    }
}

static AudioFormat detect_format(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return AUDIO_FORMAT_UNKNOWN;

    if (strcasecmp(ext, ".flac") == 0) return AUDIO_FORMAT_FLAC;
    if (strcasecmp(ext, ".ogg") == 0) return AUDIO_FORMAT_VORBIS;

    return AUDIO_FORMAT_UNKNOWN;
}

static bool open_flac(const char *path) {
    ctx.flac_file = fopen(path, "rb");
    if (!ctx.flac_file) {
        fprintf(stderr, "Failed to open file: %s\n", path);
        return false;
    }

    ctx.flac_decoder = FLAC__stream_decoder_new();
    if (!ctx.flac_decoder) {
        fprintf(stderr, "Failed to create FLAC decoder\n");
        fclose(ctx.flac_file);
        ctx.flac_file = NULL;
        return false;
    }

    FLAC__StreamDecoderInitStatus status = FLAC__stream_decoder_init_FILE(
        ctx.flac_decoder,
        ctx.flac_file,
        flac_write_callback,
        flac_metadata_callback,
        flac_error_callback,
        &ctx
    );

    if (status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
        fprintf(stderr, "Failed to initialize FLAC decoder: %s\n",
                FLAC__StreamDecoderInitStatusString[status]);
        FLAC__stream_decoder_delete(ctx.flac_decoder);
        ctx.flac_decoder = NULL;
        fclose(ctx.flac_file);
        ctx.flac_file = NULL;
        return false;
    }

    // Process metadata to get sample rate and channels
    FLAC__stream_decoder_process_until_end_of_metadata(ctx.flac_decoder);

    ctx.format = AUDIO_FORMAT_FLAC;
    return true;
}

static bool open_vorbis(const char *path) {
    int result = ov_fopen(path, &ctx.vorbis_file);
    if (result != 0) {
        fprintf(stderr, "Failed to open Vorbis file: %s (error %d)\n", path, result);
        return false;
    }

    vorbis_info *info = ov_info(&ctx.vorbis_file, -1);
    if (!info) {
        fprintf(stderr, "Failed to get Vorbis info\n");
        ov_clear(&ctx.vorbis_file);
        return false;
    }

    ctx.sample_rate = info->rate;
    ctx.channels = info->channels;
    ctx.total_samples = ov_pcm_total(&ctx.vorbis_file, -1);
    ctx.vorbis_open = true;
    ctx.format = AUDIO_FORMAT_VORBIS;

    return true;
}

bool audio_play_file(const char *path) {
    audio_stop();

    AudioFormat format = detect_format(path);
    if (format == AUDIO_FORMAT_UNKNOWN) {
        fprintf(stderr, "Unknown audio format: %s\n", path);
        return false;
    }

    bool opened = false;
    if (format == AUDIO_FORMAT_FLAC) {
        opened = open_flac(path);
    } else if (format == AUDIO_FORMAT_VORBIS) {
        opened = open_vorbis(path);
    }

    if (!opened) return false;

    // Reset buffer and position
    ctx.buffer_read_pos = 0;
    ctx.buffer_write_pos = 0;
    ctx.buffer_count = 0;
    ctx.samples_played = 0;
    ctx.finished = false;

    // Pre-fill buffer
    if (ctx.format == AUDIO_FORMAT_FLAC) {
        decode_flac_samples();
    } else {
        decode_vorbis_samples();
    }

    // Start playback
    if (ma_device_start(&ctx.device) != MA_SUCCESS) {
        fprintf(stderr, "Failed to start audio device\n");
        audio_stop();
        return false;
    }

    ctx.state = AUDIO_STATE_PLAYING;
    return true;
}

void audio_stop(void) {
    if (ctx.state != AUDIO_STATE_STOPPED) {
        ma_device_stop(&ctx.device);
    }

    if (ctx.flac_decoder) {
        FLAC__stream_decoder_finish(ctx.flac_decoder);
        FLAC__stream_decoder_delete(ctx.flac_decoder);
        ctx.flac_decoder = NULL;
        // Note: FLAC__stream_decoder_init_FILE takes ownership of the file,
        // so finish() already closed it. Don't close it again.
        ctx.flac_file = NULL;
    } else if (ctx.flac_file) {
        // Only close if decoder wasn't initialized (shouldn't happen normally)
        fclose(ctx.flac_file);
        ctx.flac_file = NULL;
    }

    if (ctx.vorbis_open) {
        ov_clear(&ctx.vorbis_file);
        ctx.vorbis_open = false;
    }

    ctx.format = AUDIO_FORMAT_UNKNOWN;
    ctx.state = AUDIO_STATE_STOPPED;
    ctx.buffer_count = 0;
}

void audio_toggle_pause(void) {
    if (ctx.state == AUDIO_STATE_PLAYING) {
        ma_device_stop(&ctx.device);
        ctx.state = AUDIO_STATE_PAUSED;
    } else if (ctx.state == AUDIO_STATE_PAUSED) {
        ma_device_start(&ctx.device);
        ctx.state = AUDIO_STATE_PLAYING;
    }
}

AudioState audio_get_state(void) {
    return ctx.state;
}

bool audio_is_finished(void) {
    return ctx.finished && ctx.buffer_count == 0;
}

// Miniaudio callback - called from audio thread
static void audio_callback(ma_device *device, void *output, const void *input, ma_uint32 frame_count) {
    (void)device;
    (void)input;

    float *out = (float *)output;
    ma_uint32 frames_written = 0;

    while (frames_written < frame_count) {
        // If buffer is empty, try to decode more
        if (ctx.buffer_count == 0) {
            bool decoded = false;
            if (ctx.format == AUDIO_FORMAT_FLAC) {
                decoded = decode_flac_samples();
            } else if (ctx.format == AUDIO_FORMAT_VORBIS) {
                decoded = decode_vorbis_samples();
            }

            if (!decoded || ctx.buffer_count == 0) {
                // End of file or error - fill rest with silence
                ma_uint32 remaining = frame_count - frames_written;
                memset(out + frames_written * 2, 0, remaining * 2 * sizeof(float));
                ctx.finished = true;
                return;
            }
        }

        // Copy samples from buffer to output
        size_t samples_available = ctx.buffer_count;
        size_t samples_needed = (frame_count - frames_written) * 2;  // stereo
        size_t samples_to_copy = samples_available < samples_needed ? samples_available : samples_needed;

        for (size_t i = 0; i < samples_to_copy; i++) {
            out[frames_written * 2 + i] = ctx.buffer[ctx.buffer_read_pos] * ctx.volume;
            ctx.buffer_read_pos = (ctx.buffer_read_pos + 1) % AUDIO_BUFFER_SIZE;
        }

        ctx.buffer_count -= samples_to_copy;
        size_t frames_copied = samples_to_copy / 2;
        ctx.samples_played += frames_copied;
        frames_written += frames_copied;
    }
}

// FLAC write callback - called when decoder has samples
static FLAC__StreamDecoderWriteStatus flac_write_callback(
    const FLAC__StreamDecoder *decoder,
    const FLAC__Frame *frame,
    const FLAC__int32 *const buffer[],
    void *client_data)
{
    (void)decoder;
    (void)client_data;

    // Store sample rate and channels from first frame
    if (ctx.sample_rate == 0) {
        ctx.sample_rate = frame->header.sample_rate;
        ctx.channels = frame->header.channels;
    }

    unsigned int bits = frame->header.bits_per_sample;
    float scale = 1.0f / (float)(1 << (bits - 1));

    for (unsigned int i = 0; i < frame->header.blocksize; i++) {
        // Check if buffer has space
        if (ctx.buffer_count >= AUDIO_BUFFER_SIZE - 2) {
            return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
        }

        // Convert to float and store (handle mono/stereo)
        if (ctx.channels == 1) {
            float sample = buffer[0][i] * scale;
            ctx.buffer[ctx.buffer_write_pos] = sample;
            ctx.buffer_write_pos = (ctx.buffer_write_pos + 1) % AUDIO_BUFFER_SIZE;
            ctx.buffer[ctx.buffer_write_pos] = sample;  // duplicate for stereo output
            ctx.buffer_write_pos = (ctx.buffer_write_pos + 1) % AUDIO_BUFFER_SIZE;
            ctx.buffer_count += 2;
        } else {
            for (unsigned int ch = 0; ch < 2 && ch < ctx.channels; ch++) {
                float sample = buffer[ch][i] * scale;
                ctx.buffer[ctx.buffer_write_pos] = sample;
                ctx.buffer_write_pos = (ctx.buffer_write_pos + 1) % AUDIO_BUFFER_SIZE;
                ctx.buffer_count++;
            }
        }
    }

    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void flac_metadata_callback(
    const FLAC__StreamDecoder *decoder,
    const FLAC__StreamMetadata *metadata,
    void *client_data)
{
    (void)decoder;
    (void)client_data;

    if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
        ctx.sample_rate = metadata->data.stream_info.sample_rate;
        ctx.channels = metadata->data.stream_info.channels;
        ctx.total_samples = metadata->data.stream_info.total_samples;
    }
}

static void flac_error_callback(
    const FLAC__StreamDecoder *decoder,
    FLAC__StreamDecoderErrorStatus status,
    void *client_data)
{
    (void)decoder;
    (void)client_data;
    fprintf(stderr, "FLAC decode error: %s\n", FLAC__StreamDecoderErrorStatusString[status]);
}

static bool decode_flac_samples(void) {
    if (!ctx.flac_decoder) return false;

    // Decode one frame
    FLAC__bool ok = FLAC__stream_decoder_process_single(ctx.flac_decoder);
    if (!ok) return false;

    FLAC__StreamDecoderState state = FLAC__stream_decoder_get_state(ctx.flac_decoder);
    return state != FLAC__STREAM_DECODER_END_OF_STREAM &&
           state != FLAC__STREAM_DECODER_ABORTED;
}

static bool decode_vorbis_samples(void) {
    if (!ctx.vorbis_open) return false;

    // Decode into temporary buffer (16-bit signed)
    char pcm_buffer[4096];
    int bitstream;

    long bytes_read = ov_read(&ctx.vorbis_file, pcm_buffer, sizeof(pcm_buffer),
                               0,  // little endian
                               2,  // 16-bit
                               1,  // signed
                               &bitstream);

    if (bytes_read <= 0) {
        return false;  // EOF or error
    }

    // Convert 16-bit samples to float
    int16_t *samples = (int16_t *)pcm_buffer;
    size_t sample_count = bytes_read / 2;

    for (size_t i = 0; i < sample_count; i++) {
        if (ctx.buffer_count >= AUDIO_BUFFER_SIZE) break;

        float sample = samples[i] / 32768.0f;

        if (ctx.channels == 1) {
            // Mono: duplicate for stereo output
            ctx.buffer[ctx.buffer_write_pos] = sample;
            ctx.buffer_write_pos = (ctx.buffer_write_pos + 1) % AUDIO_BUFFER_SIZE;
            ctx.buffer[ctx.buffer_write_pos] = sample;
            ctx.buffer_write_pos = (ctx.buffer_write_pos + 1) % AUDIO_BUFFER_SIZE;
            ctx.buffer_count += 2;
            i++;  // We processed mono, but output stereo
        } else {
            ctx.buffer[ctx.buffer_write_pos] = sample;
            ctx.buffer_write_pos = (ctx.buffer_write_pos + 1) % AUDIO_BUFFER_SIZE;
            ctx.buffer_count++;
        }
    }

    return true;
}

double audio_get_position(void) {
    if (ctx.sample_rate == 0) return 0.0;
    return (double)ctx.samples_played / (double)ctx.sample_rate;
}

double audio_get_duration(void) {
    if (ctx.sample_rate == 0) return 0.0;
    return (double)ctx.total_samples / (double)ctx.sample_rate;
}

bool audio_seek(double position) {
    if (ctx.state == AUDIO_STATE_STOPPED) return false;
    if (position < 0) position = 0;

    // Clear buffer
    ctx.buffer_read_pos = 0;
    ctx.buffer_write_pos = 0;
    ctx.buffer_count = 0;

    if (ctx.format == AUDIO_FORMAT_FLAC) {
        uint64_t sample_pos = (uint64_t)(position * ctx.sample_rate);
        if (sample_pos >= ctx.total_samples) {
            sample_pos = ctx.total_samples > 0 ? ctx.total_samples - 1 : 0;
        }
        if (!FLAC__stream_decoder_seek_absolute(ctx.flac_decoder, sample_pos)) {
            return false;
        }
        ctx.samples_played = sample_pos;
    } else if (ctx.format == AUDIO_FORMAT_VORBIS) {
        if (ov_time_seek(&ctx.vorbis_file, position) != 0) {
            return false;
        }
        ctx.samples_played = (uint64_t)(position * ctx.sample_rate);
    }

    ctx.finished = false;
    return true;
}

void audio_set_volume(float volume) {
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    ctx.volume = volume;
}

float audio_get_volume(void) {
    return ctx.volume;
}
