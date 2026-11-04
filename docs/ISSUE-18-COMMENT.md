# Ready-to-post comment for issue #18

Paste into https://github.com/GalaxyShad/Zuma-Deluxe-HD/issues/18 after pushing
the branch, replacing `<FORK_URL>` with your repository URL.

---

## Short version

I implemented audio on a fork, using only open-source pieces: SDL2 audio plus
the public-domain `stb_vorbis`. There is no BASS DLL, no libopenmpt and no codec
DLL at build or run time, and it builds with the existing README steps.

Branch: `<FORK_URL>/tree/audio-open-source`

How it works: the MO3 is a single file with eight subsongs, so each subsong is
rendered once to `content/music/track<id>.ogg` and the game plays those with the
cue ids it already uses. The `zuma.mo3` stays in the tree as the source; a
developer-only target regenerates the assets.

Music now follows state: level theme, near-hole suspense, win and game-over.
Two tracks have one-time intros and carry a `LOOPSTART` comment so looping
re-enters at the right place instead of replaying the intro.

Twelve effect clips used Vorbis floor type 0, which `stb_vorbis` cannot decode,
so they were re-encoded to supported streams under their existing names.

Verified with a chunk-invariance decode test, a per-cue test and the smoke test;
all eight cues and the effects play, and a decoded track correlates ~0.9999 with
its rendered source.

Caveat: Ogg is lossy and libopenmpt is not BASS, so this is close to the
original but not bit-exact. If you would rather keep a module player, the
`docs/AUDIO.md` in the branch explains the trade and how to regenerate.

## Longer version (post if they want detail)

- `src/global/audio_thin.c` — backend: streams music, mixes effect one-shots,
  per-voice volume and pitch, no external decoder.
- `thirdparty/stb/stb_vorbis.c` — public domain, vendored like `stb_image`.
- `src/global/Engine.c` — audio ownership, load/play/stop/volume, and teardown
  ordered so the callback stops before buffers are freed.
- `src/gameplay/Game.c` — the state machine behind the music switching, with a
  margin and confirm delay so a chain sitting on the threshold does not flap.
- `tools/` — `audio_stream_test.c` (decoding is invariant to pull size),
  `audio_cue_test.c` (every cue), `audio_smoke.c`, and `render_track.c` for
  regeneration.

Earlier dead ends worth knowing: the first renderer passed overlapping buffers
to libopenmpt's *planar* stereo API and corrupted every track, and an early
player decoded one packet per callback and discarded the remainder, which
stretched and gapped the music. Both are fixed, and the tests above exist so
they cannot regress silently.

Regeneration needs the optional `thirdparty/openmpt` submodule and ffmpeg:

```
cmake -S . -B build/audio-tools -A x64 -DZUMA_BUILD_AUDIO_RENDERER=ON
cmake --build build/audio-tools --config Release --target zuma_render_track
zuma_render_track content/music/zuma.mo3 0 build/track0.f32
ffmpeg -f f32le -ar 44100 -ac 2 -i build/track0.f32 -c:a libvorbis -q:a 10 content/music/track0.ogg
```

ffmpeg is development-time only. Normal game builds stay C-only and need
nothing extra.
