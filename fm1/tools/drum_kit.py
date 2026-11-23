#!/usr/bin/env python3
"""Build a compact ROM kit from Michael Fischer's TR-808 recordings."""
import argparse
import csv
import subprocess
from pathlib import Path

import numpy as np

SOUNDS = [
    ("kick", "bd8/BD5050.WAV", 22050, .90, .95),
    ("snare", "sd8/SD5050.WAV", 44100, .38, .82),
    ("hat", "ch8/CH.WAV", 44100, .22, .70),
    ("cowbell", "cb8/CB.WAV", 22050, .45, .68),
]

def load(path, rate):
    return np.frombuffer(subprocess.check_output([
        "ffmpeg", "-loglevel", "error", "-i", str(path), "-ac", "1",
        "-ar", str(rate), "-f", "f32le", "-",
    ]), dtype="<f4").copy()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("header", type=Path)
    parser.add_argument("metrics", type=Path)
    args = parser.parse_args()
    metrics = []
    with args.header.open("w") as header:
        print("/* Generated from Michael Fischer's TR-808 Sound Sample Set (CC0 mirror).", file=header)
        print(" * Attribution and exact source in research/README.md. */", file=header)
        for sound, name, rate, maximum, level in SOUNDS:
            x = load(args.directory / name, rate)
            peak = np.max(np.abs(x))
            active = np.flatnonzero(np.abs(x) > peak * .007)
            start = max(0, active[0] - round(.0005 * rate))
            end = min(len(x), start + round(maximum * rate), active[-1] + round(.012 * rate))
            x = x[start:end]
            fade = min(len(x) // 8, round(.008 * rate))
            x[-fade:] *= np.linspace(1, 0, fade)
            x *= 32767 * level / peak
            rendered = np.rint(x).astype(np.int16)
            nfft = 65536
            spec = abs(np.fft.rfft(x * np.hanning(len(x)), nfft))
            spec[:round(30 * nfft / rate)] = 0
            strongest = np.argmax(spec) * rate / nfft
            metrics.append([sound, name, rate, len(x), round(1000 * len(x) / rate, 1), round(strongest, 1)])
            print(f"static const int16_t drum_{sound}[] = {{", file=header)
            for i in range(0, len(rendered), 14):
                print("    " + ", ".join(str(k) for k in rendered[i:i + 14]) + ",", file=header)
            print("};", file=header)
    with args.metrics.open("w", newline="") as file:
        writer = csv.writer(file)
        writer.writerow(["sound", "recording", "sample_rate", "frames", "duration_ms", "whole_hit_fft_peak_hz"])
        writer.writerows(metrics)

if __name__ == "__main__":
    main()
