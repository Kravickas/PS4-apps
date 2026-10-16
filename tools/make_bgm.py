#!/usr/bin/env python3
"""Build bgm.wav (16-bit stereo 48 kHz PCM, what src/bgm.c plays) from an audio file.

  python3 make_bgm.py input.mp3 bgm.wav [crossfade_ms]

Decodes with ffmpeg (gapless: the LAME tag's encoder delay / padding are
trimmed), converts to 48 kHz stereo (the only rate sceAudioOutOpen takes), then
makes the loop seamless: the last F frames are blended into the first F with an
equal-power crossfade and playback wraps into frame F, so a transient or level
step at the file's start never plays at full weight (loop length N - F).
Needs ffmpeg and numpy."""
import subprocess, sys, wave
import numpy as np

def main():
    src, dst = sys.argv[1], sys.argv[2]
    ms = float(sys.argv[3]) if len(sys.argv) > 3 else 10.0
    raw = subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-i", src,
                          "-f", "s16le", "-acodec", "pcm_s16le", "-ar", "48000", "-ac", "2", "-"],
                         check=True, capture_output=True).stdout
    x = np.frombuffer(raw, np.int16).reshape(-1, 2).astype(np.float64)
    n, f = len(x), int(round(48000 * ms / 1000.0))
    if n < 4 * f:
        sys.exit("input too short for the crossfade")
    th = (np.arange(f) + 0.5) / f * (np.pi / 2)
    y = x[f:].copy()
    y[n - 2 * f:] = x[n - f:] * np.cos(th)[:, None] + x[:f] * np.sin(th)[:, None]
    peak = np.abs(y).max()
    if peak > 32767:
        sys.exit("crossfade would clip (peak %.0f)" % peak)
    out = np.round(y).astype(np.int16)
    with wave.open(dst, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(48000)
        w.writeframes(out.tobytes())
    print("%s: %d frames (%.4f s), crossfade %d frames, peak %d" % (dst, len(out), len(out) / 48000.0, f, int(peak)))

if __name__ == "__main__":
    main()
