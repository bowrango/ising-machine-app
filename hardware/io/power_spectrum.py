"""Power spectrum of Siglent scope channels (live via scope.py, or from a scope.py CSV). Needs numpy + matplotlib.

    python power_spectrum.py 1 2                          # live: plot
    python power_spectrum.py 1 --write                    # also write spectrum/spectrum_<datetime>.csv
    python power_spectrum.py --read scope_<datetime>.csv  # from a saved scope.py capture

Levels are RMS volts per bin (Hann window, corrected for tone amplitude), in dBV = 20 log10(Vrms).
Resolution is sample_rate / samples; capture more cycles (slower timebase) for finer bins.
Harmonics of the strongest tone are labelled on the plot and listed in dBc.
"""

import argparse
import csv
from datetime import datetime
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

import scope

OUT_DIR = Path(__file__).parent / "spectrum"


def spectrum(x: np.ndarray, srate: float):
    """One-sided spectrum: (freq Hz, Vrms per bin) for a tone landing on a bin."""
    w = np.hanning(len(x))
    X = np.fft.rfft((x - x.mean()) * w)
    vrms = np.abs(X) * 2 / w.sum() / np.sqrt(2)
    return np.fft.rfftfreq(len(x), 1 / srate), vrms


def harmonics(freq: np.ndarray, vrms: np.ndarray, count: int = 10):
    """[(n, freq Hz, Vrms)] for the strongest tone f0 and its harmonics n*f0 below Nyquist.

    f0 is refined between bins by a parabolic fit to the log magnitude (Hann peak);
    each harmonic is the largest bin within 5 bins of n*f0.
    """
    df = freq[1]
    db = 20 * np.log10(vrms + 1e-12)
    k = int(np.argmax(vrms[1:])) + 1
    if 0 < k < len(db) - 1:
        a, b, c = db[k - 1], db[k], db[k + 1]
        k += 0.5 * (a - c) / (a - 2 * b + c)
    f0 = k * df
    out = []
    for n in range(1, count + 1):
        if n * f0 >= freq[-1]:
            break
        i = int(round(n * f0 / df))
        lo, hi = max(i - 5, 1), min(i + 6, len(vrms))
        j = lo + int(np.argmax(vrms[lo:hi]))
        out.append((n, f0 if n == 1 else freq[j], vrms[j]))
    return out


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
    res = srate / n

    if args.write:
        OUT_DIR.mkdir(exist_ok=True)
        path = OUT_DIR / f"spectrum_{datetime.now():%Y-%m-%d_%H%M%S}.csv"
        with open(path, "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["freq"] + [f"{name}_dBV" for name in specs])
            for i in range(len(freq)):
                w.writerow([f"{freq[i]:.6e}"] + [f"{20 * np.log10(v[i] + 1e-12):.2f}" for _, v in specs.values()])

    for name, (_, vrms) in specs.items():
        line, = plt.semilogx(freq[1:], 20 * np.log10(vrms[1:] + 1e-12), label=name)
        hs = harmonics(freq, vrms)
        for h, f, v in hs:
            level = 20 * np.log10(v)
            plt.plot(f, level, "v", color=line.get_color(), ms=5)
            label = f"f0 {f / 1e6:.3f} MHz" if h == 1 else str(h)
            plt.annotate(label, (f, level), textcoords="offset points", xytext=(0, 6),
                         ha="center", va="bottom", fontsize=8, color=line.get_color())
    plt.title(f"{n} samples at {srate:.3g} Sa/s, resolution {res / 1e3:.3g} kHz")
    plt.xlabel("Frequency (Hz)")
    plt.ylabel("dBV (RMS)")
    plt.grid(True, which="both", alpha=0.3)
    plt.legend()
    plt.show()


if __name__ == "__main__":
    main()
