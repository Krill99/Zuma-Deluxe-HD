# Issue #18: thin, open-source audio implementation

The runtime uses **SDL2** (already a dependency) and the public-domain
`stb_vorbis.c` decoder. Nothing loads or links BASS or libopenmpt while playing.

The original music is a single MO3 with eight subsongs. Each subsong has been
rendered offline to a separate high-quality Ogg cue (`content/music/track<id>.ogg`).
The original MO3 remains in the repository as the conversion source; the optional
libopenmpt submodule and a developer-installed ffmpeg encoder are needed **only
if regenerating the assets**, not for playing or building the game.

Eleven original effect files use Vorbis floor type 0, which stb_vorbis cannot
read. Those clips were re-encoded to supported Vorbis streams, keeping their
names and cue IDs. Two subsongs have intros before their repeat points; track
24 and track 28 carry `LOOPSTART=829786` Vorbis comments for the thin player.

Important correction: an earlier offline renderer incorrectly supplied
`buffer` and `buffer+1` to libopenmpt's planar stereo API, corrupting *all*
tracks. It also baked in extreme width/low-pass settings. The current renderer
uses the interleaved stereo API and **does not apply that processing**. An
earlier runtime loop discarded most decoder packets or left callbacks partly
silent; it now pulls exactly the requested samples. `tools/audio_stream_test.c`
compares 1024- and 257-frame decoding of all eight cues.

This is a cross-platform, single-decoder solution, but pre-rendered Ogg is lossy
and libopenmpt is not BASS: it is **not bit-for-bit identical to the original
soundtrack**. For exact BASS output, either an independently implemented
compatible module mixer or an offline BASS reference render would be needed.
Neither is part of this open-source implementation.

See [AUDIO.md](AUDIO.md) for build/regeneration commands and the verification
results.
