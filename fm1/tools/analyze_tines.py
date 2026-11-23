#!/usr/bin/env python3
"""Measure isolated notes from a local jRhodes3d mono checkout."""
import argparse
import csv
import re
import subprocess
import sys
from pathlib import Path

import numpy as np

RATE = 44100
NAME = re.compile(r"A_(\d+)__[^_]+_(\d)\.flac")

def amplitudes(signal, frequency, start, end, harmonics):
    samples = signal[round(start * RATE):round(end * RATE)]
    window = np.hanning(len(samples))
    phase = 2j * np.pi * frequency / RATE * np.arange(len(samples))
    return np.array([
        abs(np.dot(samples * window, np.exp(h * phase))) * 2 / window.sum()
        for h in harmonics
    ])

def measure(path, note, layer):
    source = subprocess.check_output([
        "ffmpeg", "-v", "error", "-i", str(path), "-t", "3", "-ac", "1",
        "-ar", str(RATE), "-f", "f32le", "-",
    ])
    signal = np.frombuffer(source, dtype="<f4")
    peak = np.max(np.abs(signal))
    onset = np.flatnonzero(np.abs(signal) > .025 * peak)[0]
    signal = signal[onset:]
    expected = 440 * 2 ** ((note - 69) / 12)
    search = signal[round(.07 * RATE):round(.5 * RATE)]
    fft = abs(np.fft.rfft(search * np.hanning(len(search)), 262144))
    lo, hi = np.array([.97, 1.03]) * expected * 262144 / RATE
    index = int(lo) + np.argmax(fft[int(lo):int(hi) + 1])
    left, center, right = np.log(fft[index-1:index+2])
    frequency = (index + .5 * (left - right) / (left - 2 * center + right)) * RATE / 262144
    early = amplitudes(signal, frequency, .018, .095, [1, 2, 3, 4, 5])
    sustain = amplitudes(signal, frequency, .23, .41, [1, 2, 3, 4, 5])
    late = amplitudes(signal, frequency, 1.18, 1.36, [1])
    return [note, layer, round(frequency, 3), round(float(early[0]), 5),
            *np.round(20 * np.log10(early[1:] / early[0]), 2),
            *np.round(20 * np.log10(sustain[1:] / sustain[0]), 2),
            round(float(20 * np.log10(late[0] / sustain[0])), 2)]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    writer = csv.writer(sys.stdout)
    writer.writerow(["midi_note", "layer", "fundamental_hz", "attack_fundamental_fs",
                     *[f"attack_h{h}_db_rel" for h in range(2, 6)],
                     *[f"held_h{h}_db_rel" for h in range(2, 6)], "held_f0_db_per_second"])
    files = sorted(args.directory.glob("*.flac"))
    if not files:
        parser.error("no .flac recordings in directory")
    for path in files:
        if match := NAME.fullmatch(path.name):
            writer.writerow(measure(path, *map(int, match.groups())))

if __name__ == "__main__":
    main()
