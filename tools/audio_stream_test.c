/* Regression: chunked music playback must neither skip Vorbis packets nor
 * insert silence at callback boundaries. Build with stb_vorbis.c and run once
 * for each track*.ogg. No audio output device is needed. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>

static int decode(const char *path, int chunk, uint64_t *checksum,
                  uint64_t *frames, float *first) {
    int error = 0;
    stb_vorbis *decoder = stb_vorbis_open_filename(path, &error, NULL);
    float buffer[1024 * 2];
    *checksum = UINT64_C(14695981039346656037);
    *frames = 0;
    if (!decoder) {
        fprintf(stderr, "could not open %s (stb_vorbis %d)\n", path, error);
        return 0;
    }
    for (;;) {
        int got = stb_vorbis_get_samples_float_interleaved(decoder, 2,
                                                            buffer, chunk * 2);
        if (got <= 0) break;
        if (*frames == 0 && first) memcpy(first, buffer, sizeof(float) * 2);
        for (int i = 0; i < got * 2; ++i) {
            uint32_t bits;
            if (!isfinite(buffer[i])) { stb_vorbis_close(decoder); return 0; }
            memcpy(&bits, &buffer[i], sizeof(bits));
            for (int b = 0; b < 4; ++b) {
                *checksum ^= (bits >> (8 * b)) & 0xff;
                *checksum *= UINT64_C(1099511628211);
            }
        }
        *frames += (uint64_t)got;
    }
    /* The intro of tracks 24 and 28 plays once; their repeat enters at the
     * restart order. Verify the metadata survives the Vorbis encode and that
     * stb_vorbis can seek to that precise sample after reaching EOF. */
    stb_vorbis_comment tags = stb_vorbis_get_comment(decoder);
    unsigned long loopStart = 0;
    for (int i = 0; i < tags.comment_list_length; ++i) {
        const char *tag = tags.comment_list[i];
        if (tag && strncmp(tag, "LOOPSTART=", 10) == 0) {
            char *end;
            loopStart = strtoul(tag + 10, &end, 10);
            if (end == tag + 10 || *end != '\0') loopStart = 0;
        }
    }
    int hasIntro = strstr(path, "track24.ogg") || strstr(path, "track28.ogg");
    if ((hasIntro && loopStart != 829786UL) ||
        (!hasIntro && loopStart != 0) || loopStart >= *frames) {
        fprintf(stderr, "invalid LOOPSTART=%lu in %s\n", loopStart, path);
        stb_vorbis_close(decoder);
        return 0;
    }
    if (loopStart) {
        if (!stb_vorbis_seek(decoder, (unsigned int)loopStart) ||
            stb_vorbis_get_samples_float_interleaved(decoder, 2, buffer, chunk * 2) <= 0) {
            fprintf(stderr, "could not seek to LOOPSTART in %s\n", path);
            stb_vorbis_close(decoder);
            return 0;
        }
    }
    if (!stb_vorbis_seek_start(decoder)) {
        fprintf(stderr, "could not rewind %s\n", path);
        stb_vorbis_close(decoder);
        return 0;
    }
    if (first) {
        int got = stb_vorbis_get_samples_float_interleaved(decoder, 2,
                                                            buffer, chunk * 2);
        if (got <= 0 || memcmp(first, buffer, sizeof(float) * 2) != 0) {
            fprintf(stderr, "loop rewind differs at the first frame: %s\n", path);
            stb_vorbis_close(decoder);
            return 0;
        }
    }
    stb_vorbis_close(decoder);
    return *frames > 0;
}

int main(int argc, char **argv) {
    int failed = 0;
    if (argc < 2) { fprintf(stderr, "usage: %s track0.ogg ...\n", argv[0]); return 2; }
    for (int i = 1; i < argc; ++i) {
        uint64_t a, b, framesA, framesB;
        float first[2];
        int ok = decode(argv[i], 1024, &a, &framesA, first) &&
                 decode(argv[i], 257, &b, &framesB, NULL) &&
                 framesA == framesB && a == b;
        printf("%s %s frames=%llu 1024-vs-257=%s\n", argv[i],
               ok ? "PASS" : "FAIL", (unsigned long long)framesA,
               ok ? "identical" : "DIFFERENT");
        if (!ok) ++failed;
    }
    return failed ? 1 : 0;
}
