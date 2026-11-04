# Parity pass — changes and evidence

Reference: `C:\Program Files (x86)\Steam\steamapps\common\Zuma Deluxe`
(Zuma.exe SHA-256 `18B82A3B…1CE646`, bass.dll x86 v2.2).

Baseline of the tree before this pass: `build/parity-baseline/` and
`build/pre-parity-user-changes.patch`. Edits already present in the working tree
before the pass are listed first and were preserved.

## Pre-existing (not mine)

`src/Application.c`, `src/gameplay/Game.c`, `src/gameplay/Level.c`,
`src/gameplay/Level.h`, `src/gameplay/objects/BallChain.c` — earlier local fixes
(load error propagation, empty-chain guard, `>=` at path end, single-colour
spawn). Untouched in intent; adapted where my changes overlapped.

## Sound

**New `src/global/Audio.h`, `src/global/audio_thin.c`**
The original repository had no playback (`src/bass.h` contained no-op macros).
The runtime now decodes the eight pre-rendered music cues and 38 Ogg effects
through public-domain `thirdparty/stb/stb_vorbis.c` and the existing SDL2 audio
output. It does **not** load or link BASS or libopenmpt.

The first attempt at pre-rendering was corrupt: `tools/render_track.c` supplied
*overlapping* buffers to libopenmpt's planar stereo API and read them as
interleaved. It also baked an extreme width/low-pass combination into the
tracks. The renderer now uses `openmpt_module_read_interleaved_float_stereo`
and leaves the decoded module unprocessed. Eight Ogg tracks were regenerated
at Vorbis quality 10. The runtime used to throw away partial decoder packets
and fill callbacks with silence; it now calls
`stb_vorbis_get_samples_float_interleaved` until each callback is full.

`track24.ogg` and `track28.ogg` have `LOOPSTART=829786` comments so the module's
halfway restart points (orders 26 and 30) are respected after the introduction.
The renderer and OpenMPT source are developer-only; see `docs/AUDIO.md` for
reproduction and the remaining limits of module-player parity.

**`src/global/Engine.h` / `Engine.c`**
Replaced the fake BASS handle fields with an `AudioContext` plus owned
sound/effect arrays; `Engine_Init` no longer fails when audio is unavailable;
load/play/stop/volume/pitch now call the adapter; added `Engine_UpdateAudio`.
Legacy argument-less helpers resolve through a single owner pointer. Also fixed
uninitialised `fonts`/`soundsSfx`, renderer-before-window destruction, a leaked
`basePath` and unchecked `sprintf` paths.

**`src/menu/MenuMgr.c`**
The volume slider wrote straight to `BASS_ChannelSetAttribute`; it now calls
`Engine_UpdateAudio`, so changes reach every live voice.

**`src/gameplay/Game.c`**
Played the two shipped but unused danger cues: `SND_SLOWDOWN1` and
`SND_WARNING1` fire once when the chain enters the last 20 % (the existing
`SLOWFACTOR_POS` slowing window), reset per level.

## Gameplay

**`src/gameplay/objects/BallChain.c`**
* Empty chain: `BallChain_Append` dereferenced `balls[len-1]` and could never
  refill an empty chain; it now spawns a ball when the chain is empty.
* Capacity: append/insert bail out at `BALLCHAIN_MAX_LEN`.
* `Ball_Copy` dropped `isSingle`, `dx`, `dy`, so compaction lost single-colour
  state; `Ball_Init` did not initialise `dx`/`dy`.
* Boundary handling ran *before* movement and then used a slot that
  `BallChain_Destroy` had already shifted; balls are now removed before
  sampling, and path sampling is clamped to `spiralLen`.
* `BallChain_Insert` read `balls[len]` for tail insertions and could read
  `balls[len+1]`; the shift loop copied uninitialised slots, and the inserted
  ball inherited the struck ball's power-up/combo/colour state. Rewritten to
  copy only live balls, reset that state, and refresh the colour set.

**`src/gameplay/objects/Bullet.c` / `Bullet.h`**
* Offscreen expiry ignored `onScreen`, so a stale missed slot cleared the chain
  bonus every frame until reuse.
* Expiry used physical window size while bullets live in 1280×720 logical
  space; now uses the logical size.
* `Bullet_isInsertInBack` sampled the path past its end; it now takes
  `spiralLen`.
* `BulletsArr_AddBullet` had no capacity check.

**`src/gameplay/Game.c` / `Game.h`**
* `Game_Init` pointed `game->settings` into `levelMgr->settings` and mutated
  those entries in place (they are shared by every level and difficulty). It now
  copies the resolved settings into the game, preferring the level's own tuning
  entry and falling back to the difficulty profile.
* Shooting moved out of the old guard; `reloaddelay` now gates the shot, and
  `firespeed` sets bullet speed (`×1.5` for the HD coordinate scale) instead of
  the hardcoded `BULLET_SPEED`.
* Colour-selection loops got iteration guards.

**`src/gameplay/Level.c` / `Level.h`**
* Settings parser accepts `firespeed`, `mergespeed`, `reloaddelay` with the
  original defaults (`6`, `0.05`, `0`) — these attributes were silently ignored.
* Stage bounds rejected only `stageID > STAGE_COUNT` (an accepted second stage
  wrote out of bounds); progression parsing could read past `levelsLen`.
* Progress arrays were initialised with `11×11` loops over `[18][4]`; now
  `memset`.
* `LevelMgr_LoadLevels` leaked `buff`/`fp`/parser on error and read
  `sizeof(buff)` (pointer) instead of `XML_BUFF_SIZE`; stage entries and
  `xmlDepth` were not reset; `settingsID` allocations were leaked.
* `LevelMgr_GetLevelSettings` bounds-checks; `Level_GetInfo` tolerates missing
  settings.

## Build, docs, tooling

* `docs/AUDIO.md`, README audio section, `tools/render_track.c`,
  `tools/audio_smoke.c`, and `tools/audio_stream_test.c`.
* Normal x64 Release builds use C/SDL2/stb_vorbis; enabling the optional
  `ZUMA_BUILD_AUDIO_RENDERER` CMake flag adds a developer-only C++/libopenmpt
  target to regenerate assets.

## Verification

* `tools/audio_stream_test.c` compares 1024- and 257-frame decoding for all
  eight tracks; decoded frames and checksums agree.
* `tools/audio_smoke.c` checks cue switching, effect playback and live volumes.
* Corrected `track0.ogg` has positive L/R correlation (+0.896 over the first
  three seconds), where the earlier corrupted render was −0.778. Other tracks
  also now have positive correlation. The x64 game loads audio successfully.

## Known gaps (not addressed)

* Power-ups (`powerfreq`, bonus balls) are still unimplemented; the level files
  also drop `powerfreq` for several ranges.
* Dual-path levels (`curve2`) parse but only `spiral` is loaded; `Game` has one
  chain — those maps are not selectable.
* The offline source uses libopenmpt and the distributed cues use lossy Ogg;
  neither can guarantee bit-exact reproduction of the original BASS mixer.
* Extra lives are never awarded; `lives` only decreases.
* Tick rate is still one physics step per rendered frame at a nominal 62.5 Hz.
