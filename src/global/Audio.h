#ifndef ZUMA_AUDIO_H
#define ZUMA_AUDIO_H

#include <SDL.h>

/* Private, opaque handles; these are NOT native BASS handles. */
typedef struct AudioContext AudioContext;
typedef struct AudioSound AudioSound;

/* Opaque handles. The backend is stb_vorbis over pre-rendered Ogg music tracks
 * and decoded effect clips; see audio_thin.c and docs/AUDIO.md.
 * A context with no output device is returned valid but disabled.
 * Allocation failure yields NULL. ZUMA_AUDIO=off deliberately disables audio. */
AudioContext* Audio_Create(void);
void Audio_Destroy(AudioContext* audio);
int Audio_Available(const AudioContext* audio);
const char* Audio_Backend(const AudioContext* audio);
/* path is ignored by the pre-rendered backend; kept for callers. */
int Audio_LoadMusic(AudioContext* audio, const char* path);
void Audio_FreeMusic(AudioContext* audio);
AudioSound* Audio_LoadSound(AudioContext* audio, const char* path, int effect);
void Audio_FreeSound(AudioContext* audio, AudioSound* sound);
/* order is a music cue id (0, 24, 28, 32, 34, 36, 38, 39). */
int Audio_PlayMusic(AudioContext* audio, int order, float volume);
void Audio_StopMusic(AudioContext* audio);
void Audio_PlaySound(AudioContext* audio, AudioSound* sound, float volume, float semitones);
void Audio_StopSound(AudioContext* audio, AudioSound* sound);
void Audio_SetVolumes(AudioContext* audio, float music, float sound);
/* Diagnostic observations; -1 means stopped, level is a peak magnitude. */
int Audio_MusicOrder(AudioContext* audio);
Uint32 Audio_MusicLevel(AudioContext* audio);
Uint32 Audio_SoundLevel(AudioContext* audio, AudioSound* sound);
/* Number of music cues the backend knows about. */
int Audio_MusicOrderCount(AudioContext* audio);

#endif
