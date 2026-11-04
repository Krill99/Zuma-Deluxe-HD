/* Offline, open-source conversion of one Zuma MO3 subsong to raw interleaved
 * float stereo. Requires libopenmpt only while generating the Ogg assets;
 * the shipped game uses stb_vorbis and SDL2, not a module player.
 *
 * Usage: render_track <zuma.mo3> <subsong-index> <output.f32>
 * Feed output to ffmpeg -f f32le -ar 44100 -ac 2 -i output.f32 -c:a libvorbis ...
 */
#include <libopenmpt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define RENDER_RATE 44100
#define RENDER_FRAMES 4096

int main(int argc, char **argv) {
    FILE *input = NULL;
    FILE *output = NULL;
    void *data = NULL;
    openmpt_module *module = NULL;
    float buffer[RENDER_FRAMES * 2];
    long fileSize;
    size_t framesTotal = 0;
    int subsong;
    int result = 1;

    if (argc != 4) {
        fprintf(stderr, "usage: %s <zuma.mo3> <subsong-index> <output.f32>\n", argv[0]);
        return 2;
    }
    subsong = atoi(argv[2]);
    input = fopen(argv[1], "rb");
    if (!input || fseek(input, 0, SEEK_END) != 0 || (fileSize = ftell(input)) <= 0 ||
        fseek(input, 0, SEEK_SET) != 0 || (data = malloc((size_t)fileSize)) == NULL ||
        fread(data, 1, (size_t)fileSize, input) != (size_t)fileSize) {
        fprintf(stderr, "could not read module: %s\n", argv[1]);
        goto done;
    }
    module = openmpt_module_create_from_memory(data, (size_t)fileSize, NULL, NULL, NULL);
    if (!module || subsong < 0 || subsong >= openmpt_module_get_num_subsongs(module) ||
        !openmpt_module_select_subsong(module, subsong)) {
        fprintf(stderr, "could not select subsong %d\n", subsong);
        goto done;
    }
    {
        int restartOrder = (int)openmpt_module_get_restart_order(module, subsong);
        int restartRow = (int)openmpt_module_get_restart_row(module, subsong);
        double restartSeconds = openmpt_module_get_time_at_position(
            module, restartOrder, restartRow);
        fprintf(stderr, "subsong %d: %.3fs, restart order %d row %d at %.6fs\n",
            subsong, openmpt_module_get_duration_seconds(module),
            restartOrder, restartRow, restartSeconds);
    }
    output = fopen(argv[3], "wb");
    if (!output) {
        fprintf(stderr, "could not write: %s\n", argv[3]);
        goto done;
    }
    for (;;) {
        /* This API writes LRLR... directly. read_float_stereo is PLANAR and
         * must never be called with overlapping buffer/buffer+1 pointers. */
        size_t frames = openmpt_module_read_interleaved_float_stereo(
            module, RENDER_RATE, RENDER_FRAMES, buffer);
        if (!frames) break;
        if (fwrite(buffer, sizeof(float) * 2, frames, output) != frames) {
            fprintf(stderr, "write failed: %s\n", argv[3]);
            goto done;
        }
        framesTotal += frames;
    }
    if (!framesTotal || ferror(output)) {
        fprintf(stderr, "no audio rendered from subsong %d\n", subsong);
        goto done;
    }
    fprintf(stderr, "wrote %zu frames (%.3fs)\n", framesTotal,
        (double)framesTotal / RENDER_RATE);
    result = 0;
done:
    if (output && fclose(output) != 0) result = 1;
    if (module) openmpt_module_destroy(module);
    free(data);
    if (input) fclose(input);
    return result;
}
