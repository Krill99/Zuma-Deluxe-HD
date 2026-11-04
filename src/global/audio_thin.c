/* Thin audio backend: no module player and no third-party submodule.
 *
 * The soundtrack is pre-rendered once into one Ogg per track
 * (content/music/track<id>.ogg), with the original mixer curve already baked
 * in; playback is plain streaming Ogg decode. Effects are decoded once at load
 * and mixed as one-shots. Everything at runtime is stb_vorbis (public domain)
 * plus the SDL2 device the game already links. */
#include "Audio.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>

#define AUDIO_RATE 44100
#define AUDIO_CHANNELS 2
#define AUDIO_FRAMES 1024

struct AudioSound {
    float* pcm;        /* interleaved stereo frames */
    int frames;
    int effect;        /* only effects may be pitch shifted */
    int active;
    double position;
    float ratio;
    float gain;
    struct AudioSound* next;
};

struct AudioContext {
    SDL_AudioDeviceID device;
    SDL_mutex* lock;
    int initialized;

    /* Music: a streaming decoder over the current track file. */
    stb_vorbis* music;
    int musicOrder;
    int musicPlaying;
    int musicLoops;
    unsigned int musicLoopStart; /* LOOPSTART Vorbis comment, in PCM frames */
    float musicVolume;
    float soundVolume;

    AudioSound* sounds;
};

static volatile Uint32 lastMusicLevel;
static volatile Uint32 lastSoundLevel;

static float* Audio_ReadFile(const char* path, size_t* size) {
    SDL_RWops* rw = SDL_RWFromFile(path, "rb");
    Sint64 len;
    float* data;
    if (!rw) return NULL;
    len = SDL_RWsize(rw);
    if (len <= 0) { SDL_RWclose(rw); return NULL; }
    data = SDL_malloc((size_t)len);
    if (!data) { SDL_RWclose(rw); return NULL; }
    if (SDL_RWread(rw, data, (size_t)len, 1) != 1) {
        SDL_RWclose(rw);
        SDL_free(data);
        return NULL;
    }
    SDL_RWclose(rw);
    *size = (size_t)len;
    return data;
}

static void Audio_MixSound(AudioSound* s, float* out, int frames, float volume, Uint32* peak) {
    for (int i = 0; i < frames && s->active; ++i) {
        long index;
        float left, right, gain, magnitude;
        s->position += s->ratio;
        index = (long)s->position;
        if (index >= s->frames) { s->active = 0; break; }
        left = s->pcm[index * AUDIO_CHANNELS];
        right = s->pcm[index * AUDIO_CHANNELS + 1];
        gain = s->gain * volume;
        magnitude = fabsf(left) > fabsf(right) ? fabsf(left) : fabsf(right);
        if ((Uint32)(magnitude * 32767.0f) > *peak) *peak = (Uint32)(magnitude * 32767.0f);
        out[i * AUDIO_CHANNELS] += left * gain;
        out[i * AUDIO_CHANNELS + 1] += right * gain;
    }
}

/* Pull exactly one callback of interleaved music. stb_vorbis' sample API
 * retains surplus packet samples; get_frame_float did not, causing skipped
 * packets and long stretches of silence at every callback boundary. */
static int Audio_PullMusic(AudioContext* a, float* out, int frames) {
    int written = 0;
    int restartedWithoutSamples = 0;
    while (written < frames) {
        int got = stb_vorbis_get_samples_float_interleaved(a->music,
            AUDIO_CHANNELS, out + written * AUDIO_CHANNELS,
            (frames - written) * AUDIO_CHANNELS);
        if (got > 0) {
            written += got;
            restartedWithoutSamples = 0;
        } else {
            if (!a->musicLoops || restartedWithoutSamples ||
                !(a->musicLoopStart ? stb_vorbis_seek(a->music, a->musicLoopStart)
                                     : stb_vorbis_seek_start(a->music))) break;
            restartedWithoutSamples = 1;
        }
    }
    return written;
}

static void SDLCALL Audio_Callback(void* userdata, Uint8* stream, int len) {
    AudioContext* a = userdata;
    float* out = (float*)stream;
    int frames = len / (int)(sizeof(float) * AUDIO_CHANNELS);
    int samples = frames * AUDIO_CHANNELS;
    Uint32 level = 0, soundPeak = 0;

    SDL_memset(out, 0, (size_t)len);

    if (a->music && a->musicPlaying) {
        int produced = Audio_PullMusic(a, out, frames);
        (void)produced;
        for (int i = 0; i < samples; ++i) {
            float value = out[i] * a->musicVolume;
            Uint32 magnitude;
            out[i] = value;
            magnitude = (Uint32)(fabsf(value) * 32767.0f);
            if (magnitude > level) level = magnitude;
        }
    }
    lastMusicLevel = level;

    for (AudioSound* s = a->sounds; s; s = s->next)
        if (s->active)
            Audio_MixSound(s, out, frames, a->soundVolume, &soundPeak);
    lastSoundLevel = soundPeak;

    for (int i = 0; i < samples; ++i) {
        if (out[i] > 1.0f) out[i] = 1.0f;
        else if (out[i] < -1.0f) out[i] = -1.0f;
    }
}

AudioContext* Audio_Create(void) {
    AudioContext* a = SDL_calloc(1, sizeof(*a));
    SDL_AudioSpec want;
    const char* mode = SDL_getenv("ZUMA_AUDIO");
    if (!a) return NULL;
    a->musicVolume = 1.0f;
    a->soundVolume = 1.0f;
    a->musicOrder = -1;
    if (mode && SDL_strcasecmp(mode, "off") == 0) {
        SDL_Log("Audio explicitly disabled by ZUMA_AUDIO=off");
        return a;
    }
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "Audio disabled: SDL audio init failed (%s)", SDL_GetError());
        return a;
    }
    a->lock = SDL_CreateMutex();
    if (!a->lock) {
        SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "Audio disabled: no mutex");
        return a;
    }
    SDL_zero(want);
    want.freq = AUDIO_RATE;
    want.format = AUDIO_F32SYS;
    want.channels = AUDIO_CHANNELS;
    want.samples = AUDIO_FRAMES;
    want.callback = Audio_Callback;
    want.userdata = a;
    a->device = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
    if (!a->device) {
        SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "Audio disabled: no output device (%s)", SDL_GetError());
        SDL_DestroyMutex(a->lock);
        a->lock = NULL;
        return a;
    }
    a->initialized = 1;
    SDL_PauseAudioDevice(a->device, 0);
    SDL_Log("Audio enabled: stb_vorbis, pre-rendered music tracks");
    return a;
}

int Audio_Available(const AudioContext* a) { return a && a->initialized; }
const char* Audio_Backend(const AudioContext* a) { return Audio_Available(a) ? "stb_vorbis" : "disabled"; }

/* Resolves a track file next to the executable first (how the game runs), then
 * relative to the working directory. */
static int Audio_TrackPath(int order, char* out, size_t size) {
    char* basePath = SDL_GetBasePath();
    FILE* probe;
    if (basePath) {
        SDL_snprintf(out, size, "%smusic/track%d.ogg", basePath, order);
        SDL_free(basePath);
        probe = fopen(out, "rb");
        if (probe) { fclose(probe); return 1; }
    }
    SDL_snprintf(out, size, "music/track%d.ogg", order);
    probe = fopen(out, "rb");
    if (probe) { fclose(probe); return 1; }
    return 0;
}

int Audio_LoadMusic(AudioContext* a, const char* path) {
    char probe[512];
    (void)path;
    if (!a) return 0;
    if (!Audio_Available(a)) return 1;
    a->musicLoops = 1;
    /* Validate: the asset set must at least carry the base track. */
    if (!Audio_TrackPath(0, probe, sizeof(probe))) {
        SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO,
            "No music found (music/track0.ogg); audio stays silent");
        return 0;
    }
    return 1;
}

void Audio_FreeMusic(AudioContext* a) {
    if (!a) return;
    if (a->device) SDL_LockAudioDevice(a->device);
    if (a->music) {
        stb_vorbis_close(a->music);
        a->music = NULL;
    }
    a->musicPlaying = 0;
    a->musicOrder = -1;
    a->musicLoopStart = 0;
    if (a->device) SDL_UnlockAudioDevice(a->device);
}

static stb_vorbis* Audio_OpenTrack(AudioContext* a, int order) {
    static const int known[] = { 0, 24, 28, 32, 34, 36, 38, 39 };
    char path[512];
    int error = 0;
    stb_vorbis* vorbis = NULL;
    (void)a;
    /* Nearest lower known id, then track 0, so a missing cue still plays. */
    for (int i = (int)(sizeof(known) / sizeof(known[0])) - 1; i >= 0; --i) {
        if (known[i] > order) continue;
        if (!Audio_TrackPath(known[i], path, sizeof(path))) continue;
        vorbis = stb_vorbis_open_filename(path, &error, NULL);
        if (vorbis) break;
    }
    if (!vorbis) SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "No music track for id %d", order);
    return vorbis;
}

int Audio_PlayMusic(AudioContext* a, int order, float volume) {
    stb_vorbis* vorbis;
    unsigned int loopStart = 0;
    if (!Audio_Available(a) || order < 0) return 0;
    vorbis = Audio_OpenTrack(a, order);
    if (!vorbis) return 0;
    /* The module's menu and adventure cues contain a one-time introduction;
     * the LOOPSTART Vorbis comment preserves their restart order (26 or 30).
     * When absent, looping restarts the entire Ogg file. */
    stb_vorbis_comment comment = stb_vorbis_get_comment(vorbis);
    for (int i = 0; i < comment.comment_list_length; ++i) {
        const char *text = comment.comment_list[i];
        if (text && SDL_strncasecmp(text, "LOOPSTART=", 10) == 0) {
            char *end = NULL;
            unsigned long parsed = strtoul(text + 10, &end, 10);
            if (end != text + 10 && *end == '\0' &&
                parsed < stb_vorbis_stream_length_in_samples(vorbis))
                loopStart = (unsigned int)parsed;
            break;
        }
    }
    SDL_LockAudioDevice(a->device);
    if (a->music) stb_vorbis_close(a->music);
    a->music = vorbis;
    a->musicLoopStart = loopStart;
    a->musicVolume = volume;
    a->musicOrder = order;
    a->musicPlaying = 1;
    SDL_UnlockAudioDevice(a->device);
    return 1;
}

void Audio_StopMusic(AudioContext* a) {
    if (!Audio_Available(a)) return;
    SDL_LockAudioDevice(a->device);
    a->musicPlaying = 0;
    SDL_UnlockAudioDevice(a->device);
}

AudioSound* Audio_LoadSound(AudioContext* a, const char* path, int effect) {
    float* file;
    size_t size = 0;
    AudioSound* sound;
    stb_vorbis* vorbis;
    stb_vorbis_info info;
    int error = 0;
    int consumed = 0;
    float** channelData;
    int channels = 0;
    int got = 0;
    if (!Audio_Available(a) || !path) return NULL;

    file = Audio_ReadFile(path, &size);
    if (!file) { SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "Sound read failed: %s", path); return NULL; }
    vorbis = stb_vorbis_open_pushdata((const unsigned char*)file, (int)size, &consumed, &error, NULL);
    if (!vorbis) {
        SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "Sound decode failed: %s (stb_vorbis %d)", path, error);
        SDL_free(file);
        return NULL;
    }
    info = stb_vorbis_get_info(vorbis);
    (void)info;
    sound = SDL_calloc(1, sizeof(*sound));
    if (!sound) { stb_vorbis_close(vorbis); SDL_free(file); return NULL; }
    sound->effect = effect != 0;
    sound->ratio = 1.0f;
    sound->gain = 1.0f;

    while (consumed < (int)size) {
        int length = stb_vorbis_decode_frame_pushdata(vorbis,
            (const unsigned char*)file + consumed, (int)size - consumed,
            &channels, &channelData, &got);
        if (length <= 0) break;
        consumed += length;
        if (got > 0 && channels > 0) {
            float* grown = SDL_realloc(sound->pcm,
                ((size_t)sound->frames + (size_t)got) * AUDIO_CHANNELS * sizeof(float));
            if (!grown) break;
            sound->pcm = grown;
            for (int i = 0; i < got; ++i) {
                float left = channelData[0][i];
                float right = channels > 1 ? channelData[1][i] : left;
                sound->pcm[(sound->frames + i) * AUDIO_CHANNELS] = left;
                sound->pcm[(sound->frames + i) * AUDIO_CHANNELS + 1] = right;
            }
            sound->frames += got;
        }
    }
    stb_vorbis_close(vorbis);
    SDL_free(file);

    if (!sound->frames) {
        SDL_free(sound->pcm);
        SDL_free(sound);
        SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "Sound decoded to no frames: %s", path);
        return NULL;
    }
    SDL_LockAudioDevice(a->device);
    sound->next = a->sounds;
    a->sounds = sound;
    SDL_UnlockAudioDevice(a->device);
    return sound;
}

void Audio_FreeSound(AudioContext* a, AudioSound* s) {
    AudioSound** link;
    if (!a || !s) return;
    SDL_LockAudioDevice(a->device);
    link = &a->sounds;
    while (*link && *link != s) link = &(*link)->next;
    if (*link) *link = s->next;
    SDL_UnlockAudioDevice(a->device);
    SDL_free(s->pcm);
    SDL_free(s);
}

void Audio_PlaySound(AudioContext* a, AudioSound* s, float volume, float semitones) {
    if (!Audio_Available(a) || !s || !s->pcm) return;
    if (!(semitones >= -48.0f && semitones <= 48.0f)) semitones = 0.0f;
    SDL_LockAudioDevice(a->device);
    s->position = 0.0;
    s->ratio = s->effect ? powf(2.0f, semitones / 12.0f) : 1.0f;
    s->gain = volume;
    s->active = 1;
    SDL_UnlockAudioDevice(a->device);
}

void Audio_StopSound(AudioContext* a, AudioSound* s) {
    if (!Audio_Available(a) || !s) return;
    SDL_LockAudioDevice(a->device);
    s->active = 0;
    SDL_UnlockAudioDevice(a->device);
}

void Audio_SetVolumes(AudioContext* a, float music, float sound) {
    if (!Audio_Available(a)) return;
    a->musicVolume = music;
    a->soundVolume = sound;
}

int Audio_MusicOrder(AudioContext* a) {
    if (!Audio_Available(a) || !a->music || !a->musicPlaying) return -1;
    return a->musicOrder;
}

int Audio_MusicOrderCount(AudioContext* a) {
    (void)a;
    return 8;
}

Uint32 Audio_MusicLevel(AudioContext* a) {
    if (!Audio_Available(a) || !a->music) return 0;
    return lastMusicLevel;
}

Uint32 Audio_SoundLevel(AudioContext* a, AudioSound* s) {
    if (!Audio_Available(a) || !s) return 0;
    return lastSoundLevel;
}

void Audio_Destroy(AudioContext* a) {
    if (!a) return;
    /* Stop the callback before freeing any buffers or decoder it reads. */
    if (a->device) SDL_CloseAudioDevice(a->device);
    while (a->sounds) {
        AudioSound* next = a->sounds->next;
        SDL_free(a->sounds->pcm);
        SDL_free(a->sounds);
        a->sounds = next;
    }
    if (a->music) stb_vorbis_close(a->music);
    if (a->lock) SDL_DestroyMutex(a->lock);
    SDL_free(a);
}
