"""Compare two 44.1 kHz stereo float32 captures (for example old/new Ogg).

Requires numpy, but is only a developer check; the game itself needs no Python.
Use ffmpeg -t 3 -i track.ogg -f f32le -ar 44100 -ac 2 track.f32 first.
"""
import sys
import numpy as np

for name, path in zip(('old', 'corrected'), sys.argv[1:3]):
    audio = np.fromfile(path, dtype='<f4').reshape(-1, 2)
    rms = np.sqrt(np.mean(audio.astype('float64') ** 2, axis=0))
    corr = np.corrcoef(audio.T)[0, 1]
    print(f'{name}: frames={len(audio)}, RMS(L/R)={rms[0]:.4f}/{rms[1]:.4f}, L/R correlation={corr:+.4f}')

if len(sys.argv) == 4:
    rendered = np.fromfile(sys.argv[3], dtype='<f4').reshape(-1, 2)
    corrected = np.fromfile(sys.argv[2], dtype='<f4').reshape(-1, 2)
    frames = min(len(rendered), len(corrected))
    rendered, corrected = rendered[:frames], corrected[:frames]
    correlation = np.corrcoef(rendered.ravel(), corrected.ravel())[0, 1]
    relative_error = np.sqrt(np.mean((rendered - corrected) ** 2)) / np.sqrt(np.mean(rendered ** 2))
    print(f'corrected Ogg vs renderer: frames={frames}, correlation={correlation:.6f}, relative RMS error={relative_error:.5f}')
