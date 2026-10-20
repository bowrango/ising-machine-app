"""Power spectrum of Siglent scope channels (live via scope.py, or from a scope.py CSV). Needs numpy + matplotlib.

    python power_spectrum.py 1 2                          # live: plot
    python power_spectrum.py 1 --write                    # also write spectrum_<datetime>.csv
    python power_spectrum.py --read scope_<datetime>.csv  # from a saved scope.py capture

Levels are RMS volts per bin (Hann window, corrected for tone amplitude), in dBV = 20 log10(Vrms).
Resolution is sample_rate / samples; capture more cycles (slower timebase) for finer bins.
"""

import argparse
import csv
from datetime import datetime

import matplotlib.pyplot as plt
import numpy as np

import scope


def spectrum(x: np.ndarray, srate: float):
    """One-sided spectrum: (freq Hz, Vrms per bin) for a tone landing on a bin."""
    w = np.hanning(len(x))
    X = np.fft.rfft((x - x.mean()) * w)
    vrms = np.abs(X) * 2 / w.sum() / np.sqrt(2)
    return np.fft.rfftfreq(len(x), 1 / srate), vrms


def load_csv(path: str):
    with open(path) as f:
        rows = list(csv.reader(f))
    names, data = rows[0], np.array(rows[1:], dtype=float)
    return {name: data[:, i] for i, name in enumerate(names) if i}, 1 / np.mean(np.diff(data[:, 0]))


def main():
    p = argparse.ArgumentParser()
    p.add_argument("channels", type=int, nargs="*", choices=[1, 2, 3, 4])
    p.add_argument("--read")
    p.add_argument("--host", default=scope.HOST)
    p.add_argument("--write", action="store_true")
    args = p.parse_args()

    if args.read:
        waves, srate = load_csv(args.read)
    else:
        assert args.channels, "give at least one channel"
        _, waves, srate = scope.read(args.host, args.channels)
        waves = {f"C{ch}": np.array(w) for ch, w in waves.items()}

    specs = {name: spectrum(x, srate) for name, x in waves.items()}
    freq = next(iter(specs.values()))[0]
    n = len(next(iter(waves.values())))
    print(f"{n} samples at {srate:.3g} Sa/s, resolution {srate / n / 1e3:.3g} kHz")

    if args.write:
        path = f"spectrum_{datetime.now():%Y-%m-%d_%H%M%S}.csv"
        with open(path, "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["freq"] + [f"{name}_dBV" for name in specs])
            for i in range(len(freq)):
                w.writerow([f"{freq[i]:.6e}"] + [f"{20 * np.log10(v[i] + 1e-12):.2f}" for _, v in specs.values()])
        print(f"wrote {path}")

    for name, (_, vrms) in specs.items():
        plt.semilogx(freq[1:], 20 * np.log10(vrms[1:] + 1e-12), label=name)
    plt.xlabel("Frequency (Hz)")
    plt.ylabel("dBV (RMS)")
    plt.grid(True, which="both", alpha=0.3)
    plt.legend()
    plt.show()


if __name__ == "__main__":
    main()
