/* Check that every game music ID selects a playable pre-rendered cue.
 * Run with the music/ directory beside the executable or as the working dir. */
#include <SDL.h>
#include <stdio.h>
#include "global/Audio.h"

int main(void) {
    static const int ids[] = {0, 24, 28, 32, 34, 36, 38, 39};
    AudioContext *audio = Audio_Create();
    int failures = 0;
    if (!audio || !Audio_Available(audio) || !Audio_LoadMusic(audio, "music/zuma.mo3")) {
        fprintf(stderr, "could not initialize audio or find tracks\n");
        Audio_Destroy(audio);
        return 1;
    }
    for (int i = 0; i < 8; ++i) {
        int ok = Audio_PlayMusic(audio, ids[i], 1.0f);
        Uint32 level = 0;
        Uint32 start = SDL_GetTicks();
        while (ok && !level && SDL_GetTicks() - start < 1000) {
            SDL_Delay(20);
            level = Audio_MusicLevel(audio);
        }
        ok = ok && Audio_MusicOrder(audio) == ids[i] && level != 0;
        printf("id=%d %s level=%u\n", ids[i], ok ? "PASS" : "FAIL", level);
        if (!ok) ++failures;
    }
    Audio_Destroy(audio);
    return failures ? 1 : 0;
}
