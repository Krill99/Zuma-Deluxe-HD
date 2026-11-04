#include "Application.h"

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <string.h>

/* --level=<n> [--difficulty=<n>] jumps straight into a level, which makes
 * single-level problems reproducible without driving the menus. The values are
 * passed on through the environment that Application already reads. */
static void FromCommandLine(int argc, char **args) {
    for (int i = 1; i < argc; i++) {
        if (strncmp(args[i], "--level=", 8) == 0)
            SDL_setenv("ZUMA_START_LEVEL", args[i] + 8, 1);
        else if (strncmp(args[i], "--difficulty=", 13) == 0)
            SDL_setenv("ZUMA_START_DIFF", args[i] + 13, 1);
    }
}

int main(int argc, char ** args) {
    FromCommandLine(argc, args);
    return ApplicationZuma_Start();
}