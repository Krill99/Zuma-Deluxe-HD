# Audio: thin open-source runtime

The game ships eight pre-rendered Ogg music cues and uses SDL2 plus the
public-domain `thirdparty/stb/stb_vorbis.c` to play them. **No BASS DLL,
libopenmpt, C++ runtime, or codec DLL is needed to build or play the game.**
The original `content/music/zuma.mo3` remains available as the source asset;
its SHA-256 matches the installed Zuma Deluxe module.

## Why the music was different

The previous offline renderer passed overlapping pointers (`buffer` and
`buffer+1`) to libopenmpt's **planar** `read_float_stereo` function and then
interpreted the result as interleaved samples. It also applied a mid/side width
of 26, a one-pole low-pass with a measured cutoff of about 525 Hz, and hard
limiting. Those operations badly changed the music. The old stereo signal was
mostly anti-correlated (track 0: −0.778 over its first three seconds).

`tools/render_track.c` now calls the **interleaved** rendering API and leaves the
module's samples unaltered. The eight replacement tracks are encoded as high
quality Ogg Vorbis (`-q:a 10`); track 0 has +0.896 L/R correlation over the
same interval. Decoded track 0 correlates 0.999915 with its correctly rendered
source PCM (first three seconds, 1.4% relative RMS encoding error). A fresh
build of the optional renderer from the pinned, clean OpenMPT submodule produced
the exact same PCM SHA-256 as the source used for these assets. Ogg is lossy and
libopenmpt is a different module player from BASS: this improves parity
substantially, **but is not bit-exact BASS output**.

There was also a runtime timing bug: decoding a whole Vorbis packet for each
1024-frame SDL callback discarded any surplus samples and left the callback
partly silent when the packet was shorter than 1024 frames. Playback now uses
`stb_vorbis_get_samples_float_interleaved`, which buffers packet leftovers and
fills each callback. `tools/audio_stream_test.c` verifies the complete decoded
stream is identical when pulled in 1024-frame and 257-frame chunks.

## Cues and looping

| Module subsong | Music ID / Ogg file | Repeat starts at |
|---:|---|---|
| 0 | `track0.ogg` | start |
| 1 | `track24.ogg` | frame 829786 (order 26, row 0) |
| 2 | `track28.ogg` | frame 829786 (order 30, row 0) |
| 3 | `track32.ogg` | start |
| 4 | `track34.ogg` | start |
| 5 | `track36.ogg` | start |
| 6 | `track38.ogg` | start |
| 7 | `track39.ogg` | start |

Tracks 24 and 28 have a one-time introduction. Their Ogg files include a
`LOOPSTART` Vorbis comment, which the player reads and seeks to at EOF so the
introduction does not repeat. When the comment is absent, the whole track loops.
Unmatched IDs fall back to the nearest lower known cue.

## Regenerating the assets

The game build does not link libopenmpt, but the **optional developer tool**
uses the pinned `thirdparty/openmpt` submodule to decode the original module:

```powershell
cmake -S . -B build/audio-tools -A x64 -DZUMA_BUILD_AUDIO_RENDERER=ON
cmake --build build/audio-tools --config Release --target zuma_render_track
build/audio-tools/bin/Release/zuma_render_track.exe content/music/zuma.mo3 0 build/track0.f32
ffmpeg -f f32le -ar 44100 -ac 2 -i build/track0.f32 -c:a libvorbis -q:a 10 content/music/track0.ogg
```

Repeat for all subsongs and IDs in the table. When encoding subsongs 1 and 2,
pass `-metadata LOOPSTART=829786` to ffmpeg. ffmpeg is **a development-time
encoder**, not a runtime dependency. The `thirdparty/openmpt` submodule is
optional for normal builds; a cold developer-tools build is substantially
slower than building the game alone.

Eleven original effects use Vorbis floor type 0, which stb_vorbis cannot decode.
The corresponding `content/sounds/*.ogg` files were re-encoded to supported
Vorbis streams, preserving their names. If those assets are replaced, validate
all of them again before shipping.

## Platforms

Windows and Linux are both supported and built by CI
(`.github/workflows/build.yml`, windows-latest and ubuntu-latest). The backend is
plain C with SDL2 and `stb_vorbis`, so no platform branch is needed: music paths
come from `SDL_GetBasePath()` with `/` separators, which work on both.

On Linux prefer `-DZUMA_USE_SYSTEM_SDL2=ON` (see the README) rather than building
the vendored SDL2; `tools/verify-linux.sh` runs the CI build and every decode
test in one step. For headless runs use `SDL_VIDEODRIVER=dummy` and, if there is
no sound server, `ZUMA_AUDIO=off`.

## Verification

- `tools/audio_stream_test.c`: all eight tracks decode identically at 1024- and
  257-frame pull sizes, and can seek back to the start after EOF.
- `tools/audio_smoke.c`: music cue switching, effects, live volumes and output
  device.
- x64 Release build in `build/bin/Release`: no BASS or libopenmpt dependency.
- Linux: full build plus all eight decode tests pass in WSL Ubuntu
  (`tools/verify-linux.sh`), and the binary starts under `SDL_VIDEODRIVER=dummy`.

`ZUMA_AUDIO=off` deliberately disables sound. Audio failures are logged to
stderr; the game should continue to run when an output device is unavailable.
