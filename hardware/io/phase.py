"""Frequency of each scope channel and its phase relative to the first. Standard library only.

    python phase.py 1 2          # A_L on CH1, B_L on CH2: f_A, f_B, phase of B vs A
    python phase.py 1 2 3 4

Runs the scope briefly, stops it so every channel comes from the same acquisition, reads it, and
goes back to Run. Two oscillators are locked when their frequencies match; then the phase is
meaningful (~0 deg in phase, ~180 deg antiphase). Unlocked channels drift, so their phase is noise.
"""

import argparse
import cmath
import math
import socket
import time

import scope

TDIV = 20e-6  # 280 us capture: ~290 cycles of a ~1 MHz oscillator


def capture(host: str, channels: list[int], tdiv: float = TDIV):
    """Run the scope for a moment at tdiv, stop it on a fresh acquisition, read channels, and go back
    to Run. One connection throughout: the scope's SCPI server locks up under rapid reconnects."""
    with socket.create_connection((host, scope.PORT), timeout=10) as s:
        f = s.makefile("rwb")

        def send(cmd):
            f.write(cmd.encode() + b"\n")
            f.flush()

        send("CHDR OFF")
        send(f"TDIV {tdiv:.3E}")
        send("TRMD AUTO")
        time.sleep(0.5)
        send("STOP")
        time.sleep(0.2)
        try:
            return scope.read_from(f, channels)
        finally:
            send("TRMD AUTO")


def frequency(t: list[float], x: list[float]) -> float:
    """Mean frequency from rising crossings of the waveform's mean. A crossing only counts after the
    signal has dipped below mean - 20% of its swing, so noise near the mean isn't counted twice."""
    mid = sum(x) / len(x)
    low = mid - 0.2 * (max(x) - min(x))
    crossings, armed = [], False
    for i in range(len(x) - 1):
        if x[i] < low:
            armed = True
        elif armed and x[i] < mid <= x[i + 1]:
            crossings.append(t[i] + (mid - x[i]) / (x[i + 1] - x[i]) * (t[i + 1] - t[i]))
            armed = False
    if len(crossings) < 3:
        return 0.0
    return (len(crossings) - 1) / (crossings[-1] - crossings[0])


def phasor(t: list[float], x: list[float], f: float) -> complex:
    """Complex amplitude of x at f (single-bin DFT over the capture)."""
    mid = sum(x) / len(x)
    return sum((v - mid) * cmath.exp(-2j * math.pi * f * ti) for ti, v in zip(t, x)) * 2 / len(x)


def measure(host: str, channels: list[int], tdiv: float = TDIV, decimate: int = 4):
    """{ch: (freq Hz, amplitude Vpk, phase deg relative to channels[0] at its frequency)}."""
    t, waves, _ = capture(host, channels, tdiv)
    t = t[::decimate]
    waves = {ch: w[::decimate] for ch, w in waves.items()}
    f_ref = frequency(t, waves[channels[0]])
    ref = phasor(t, waves[channels[0]], f_ref)
    out = {}
    for ch in channels:
        p = phasor(t, waves[ch], f_ref)
        dphi = math.degrees(cmath.phase(p / ref)) if abs(ref) > 0 else float("nan")
        out[ch] = (frequency(t, waves[ch]), abs(p), dphi)
    return out


def main():
    p = argparse.ArgumentParser()
    p.add_argument("channels", type=int, nargs="+", choices=[1, 2, 3, 4])
    p.add_argument("--tdiv", type=float, default=TDIV)
    p.add_argument("--host", default=scope.HOST)
    args = p.parse_args()

    res = measure(args.host, args.channels, args.tdiv)
    f_ref = res[args.channels[0]][0]
    for ch, (f, amp, dphi) in res.items():
        print(f"C{ch}: {f / 1e6:.5f} MHz ({(f - f_ref) / 1e3:+.2f} kHz)  {amp:.3g} Vpk  {dphi:+6.1f} deg")


if __name__ == "__main__":
    main()
