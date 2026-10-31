"""
Arm a single edge trigger on the scope, wait for manual switch on, then read the capture.

    python transient.py 1                       # trigger on CH1 rising through 2 V, capture CH1
    python transient.py 1 --level 1 --write     # trigger at 1 V, save a CSV
    python transient.py 1 --tdiv 20e-6          # 20 us/div: 280 us capture
    python transient.py 1 --vdiv 1 --center 3   # 1 V/div centred on 3 V, if the waveform clips
"""

import argparse
import csv
import socket
import time
from datetime import datetime

import scope

POLL = 0.5  # seconds between trigger-status queries
PRE = 2e-6  # seconds shown before the trigger

def capture(host: str, ch: int, level: float, tdiv: float, vdiv: float, center: float):
    """Setup single-shot rising-edge trigger on ch, arm it, and block until it has triggered and stopped.
    ch is set to vdiv V/div with center V mid-screen. Uses one connection throughout"""
    delay = PRE - scope.GRID / 2 * tdiv  # scope.read: t0 = -delay - tdiv * GRID / 2 = -PRE
    with socket.create_connection((host, scope.PORT), timeout=5) as s:
        f = s.makefile("rw", newline="\n")

        def send(cmd):
            f.write(cmd + "\n")
            f.flush()

        def query(cmd):
            send(cmd)
            return f.readline().strip()

        for cmd in [
            "CHDR OFF",
            "STOP",
            f"TDIV {tdiv:.3E}",
            f"TRDL {delay:.3E}S",
            f"C{ch}:VDIV {vdiv:.3E}V",
            f"C{ch}:OFST {-center:.3E}V",  # scope.read: volts = raw - offset
            f"TRSE EDGE,SR,C{ch},HT,OFF",
            f"C{ch}:TRCP DC",
            f"C{ch}:TRSL POS",
            f"C{ch}:TRLV {level:.3E}V",
        ]:
            send(cmd)
        # The scope clamps the level to the screen (+-4.5 div around the offset).
        actual = scope.number(query(f"C{ch}:TRLV?"))
        if abs(actual - level) > 1e-3:
            print(f"Trigger level clamped to {actual:.3g} V (screen range); change V/div or offset to reach {level:.3g} V")

        send("TRMD SINGLE")  # arms
        time.sleep(POLL)  # TRMD? can still read STOP for a moment after arming
        print("Armed: switch the oscillators on (Ctrl-C to cancel)")
        # Poll TRMD? (SINGLE while armed, STOP once captured)
        while query("TRMD?").upper() != "STOP":
            time.sleep(POLL)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("channel", type=int, choices=[1, 2, 3, 4])
    p.add_argument("--level", type=float, default=2, help="trigger level (V)")
    p.add_argument("--tdiv", type=float, default=3e-6, help="timebase (s/div)")
    p.add_argument("--vdiv", type=float, default=0.5, help="vertical scale (V/div)")
    p.add_argument("--center", type=float, default=2, help="voltage at mid-screen (V)")
    p.add_argument("--host", default=scope.HOST)
    p.add_argument("--write", action="store_true")
    p.add_argument("--plot", action="store_true")
    args = p.parse_args()

    # Setup and capture trigger
    capture(args.host, args.channel, args.level, args.tdiv, args.vdiv, args.center)

    t, waves, srate = scope.read(args.host, [args.channel])
    w = waves[args.channel]
    n = len(t)
    print(f"Triggered: {n} samples at {srate:.3g} Sa/s, {t[0] * 1e6:.1f} to {t[-1] * 1e6:.1f} us")
    # The ADC saturates ~5 div from centre (2.48 V at 0.5 V/div, centre 0).
    top, bottom = args.center + 4.8 * args.vdiv, args.center - 4.8 * args.vdiv
    clipped = " (clipped: raise --vdiv or move --center)" if max(w) >= top or min(w) <= bottom else ""
    print(f"C{args.channel}: {min(w):.3g} to {max(w):.3g} V{clipped}")

    if args.write:
        path = f"transient_{datetime.now():%Y-%m-%d_%H%M%S}.csv"
        with open(path, "w", newline="") as out:
            writer = csv.writer(out)
            writer.writerow(["time", f"C{args.channel}"])
            for i in range(n):
                writer.writerow([f"{t[i]:.6e}", f"{w[i]:.5g}"])
        print(f"wrote {path}")

    if args.plot:
        import matplotlib.pyplot as plt

        plt.plot([x * 1e6 for x in t], w, lw=0.8)
        plt.axvline(0, color="k", lw=0.5, ls="--")
        plt.xlabel("time from trigger (us)")
        plt.ylabel("V")
        plt.title(f"C{args.channel} start-up, trigger rising {args.level} V")
        plt.grid(alpha=0.3)
        plt.show()


if __name__ == "__main__":
    main()
