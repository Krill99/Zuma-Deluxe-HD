/* Offline check of the optional audio backend against a caller-supplied BASS 2.2
 * library. Usage: audio_smoke <game-data-dir>
 * Exits non-zero on a functional failure; an unavailable backend is reported
 * separately so the game's silent fallback stays distinguishable. */
#include <SDL.h>
#include <stdio.h>
#include <string.h>

#include "global/Audio.h"

static int failures = 0;

static void Check(int ok, const char* what) {
    printf("%-46s %s\n", what, ok ? "PASS" : "FAIL");
    if (!ok) failures++;
}

int main(int argc, char** argv) {
    char path[512];
    AudioSound* sound;
    int order;
    Uint32 level = 0;
    Uint32 start;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <game-data-dir>\n", argv[0]);
        return 2;
    }
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");

    AudioContext* a = Audio_Create();
    Check(a != NULL, "context allocated");
    if (!a) return 1;

    printf("backend: %s\n", Audio_Backend(a));
    if (!Audio_Available(a)) {
        printf("RESULT: backend unavailable (fallback path covered)\n");
        Audio_Destroy(a);
        return 3;
    }

    SDL_snprintf(path, sizeof(path), "%s/music/zuma.mo3", argv[1]);
    Check(Audio_LoadMusic(a, path), "music loads");
    /* Pre-rendered backends ignore the argument, so a second load must work. */
    Check(Audio_LoadMusic(a, path), "music load is repeatable");

    Check(Audio_PlayMusic(a, 24, 0.5f), "seek to a mid track and play");
    SDL_Delay(150);
    order = Audio_MusicOrder(a);
    Check(order == 24, "mid track reported back");
    printf("  observed order: %d of %d\n", order, Audio_MusicOrderCount(a));

    Check(Audio_PlayMusic(a, 38, 0.5f), "seek to a later track id");
    SDL_Delay(60);
    order = Audio_MusicOrder(a);
    Check(order == 38, "later id reported back");
    printf("  observed order: %d\n", order);

    Check(Audio_PlayMusic(a, 0, 0.5f), "switch back to the gameplay cue");
    SDL_Delay(60);
    Check(Audio_MusicOrder(a) == 0, "gameplay cue reported back");

    SDL_snprintf(path, sizeof(path), "%s/sounds/pop.ogg", argv[1]);
    sound = Audio_LoadSound(a, path, 0);
    Check(sound != NULL, "sample loads");
    Audio_PlaySound(a, sound, 1.0f, 0.0f);
    start = SDL_GetTicks();
    while (level == 0 && SDL_GetTicks() - start < 300) {
        SDL_Delay(10);
        level = Audio_SoundLevel(a, sound);
    }
    Check(level > 0, "playing sample reports signal level");
    Audio_StopSound(a, sound);

    Audio_SetVolumes(a, 0.25f, 0.25f);
    Check(Audio_MusicLevel(a) > 0, "music still rendering after volume change");

    Audio_FreeSound(a, sound);
    Audio_FreeMusic(a);
    Audio_Destroy(a);

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "OK",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
