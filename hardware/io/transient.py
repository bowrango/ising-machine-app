"""
Arm a single edge trigger on the scope, wait for manual switch on, then read the capture.

    python transient.py 1                       # trigger on CH1 rising through LEVEL, capture CH1
    python transient.py 1 --write               # also save a CSV
    python transient.py 1 --plot --sim          # overlay the LTspice model (hardware/sim/oscillator.raw)
    python transient.py 1 --read transient_<datetime>.csv --sim   # re-plot a saved capture with the model
"""

import argparse
import csv
import socket
import time
from datetime import datetime

import ltspice
import scope

POLL = 0.5  # seconds between trigger-status queries
PRE = 2e-6  # seconds shown before the trigger
LEVEL = 2.0  # trigger level (V): rising edge on the start-up waveform
TDIV = 5e-6  # timebase (s/div), a 1-2-5 step the scope accepts as is: 70 us capture
VDIV = 0.5  # vertical scale (V/div)
CENTER = 2.0  # voltage at mid-screen (V): 0.5 V/div shows about -0.4 to 4.4 V
ALIGN = 1.0  # with --sim, t = 0 where each trace first rises through this level (the turn-on edge)

def capture(host: str, ch: int):
    """Setup single-shot rising-edge trigger on ch, arm it, and block until it has triggered and stopped.
    ch is set to VDIV V/div with CENTER V mid-screen. Uses one connection throughout"""
    delay = PRE - scope.GRID / 2 * TDIV  # scope.read: t0 = -delay - TDIV * GRID / 2 = -PRE
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
            f"TDIV {TDIV:.3E}",
            f"TRDL {delay:.3E}S",
            f"C{ch}:VDIV {VDIV:.3E}V",
            f"C{ch}:OFST {-CENTER:.3E}V",  # scope.read: volts = raw - offset
            f"TRSE EDGE,SR,C{ch},HT,OFF",
            f"C{ch}:TRCP DC",
            f"C{ch}:TRSL POS",
            f"C{ch}:TRLV {LEVEL:.3E}V",
        ]:
            send(cmd)
        # The scope clamps the level to the screen (+-4.5 div around the offset).
        actual = scope.number(query(f"C{ch}:TRLV?"))
        if abs(actual - LEVEL) > 1e-3:
            print(f"Trigger level clamped to {actual:.3g} V (screen range); change V/div or offset to reach {LEVEL:.3g} V")

        send("TRMD SINGLE")  # arms
        time.sleep(POLL)  # TRMD? can still read STOP for a moment after arming
        print("Armed: switch the oscillators on (Ctrl-C to cancel)")
        # Poll TRMD? (SINGLE while armed, STOP once captured)
        while query("TRMD?").upper() != "STOP":
            time.sleep(POLL)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("channel", type=int, choices=[1, 2, 3, 4])
    p.add_argument("--host", default=scope.HOST)
    p.add_argument("--write", action="store_true")
    p.add_argument("--plot", action="store_true")
    p.add_argument("--sim", nargs="?", const=ltspice.SIM_RAW, help="overlay an LTspice .raw (implies --plot)")
    p.add_argument("--sim-node", default="V(l)", help="simulated node to overlay (default V(l), the A_L side)")
    p.add_argument("--read", help="plot a CSV saved with --write instead of capturing")
    args = p.parse_args()

    if args.read:
        with open(args.read) as f:
            rows = list(csv.reader(f))[1:]
        t, w = [float(r[0]) for r in rows], [float(r[1]) for r in rows]
        n = len(t)
    else:
        # Setup and capture trigger
        capture(args.host, args.channel)

        t, waves, srate = scope.read(args.host, [args.channel])
        w = waves[args.channel]
        n = len(t)
        print(f"Triggered: {n} samples at {srate:.3g} Sa/s, {t[0] * 1e6:.1f} to {t[-1] * 1e6:.1f} us")
        # The ADC saturates ~5 div from centre (2.48 V at 0.5 V/div, centre 0).
        top, bottom = CENTER + 4.8 * VDIV, CENTER - 4.8 * VDIV
        clipped = " (clipped: raise VDIV or move CENTER)" if max(w) >= top or min(w) <= bottom else ""
        print(f"C{args.channel}: {min(w):.3g} to {max(w):.3g} V{clipped}")

    if args.write and not args.read:
        path = f"transient_{datetime.now():%Y-%m-%d_%H%M%S}.csv"
        with open(path, "w", newline="") as out:
            writer = csv.writer(out)
            writer.writerow(["time", f"C{args.channel}"])
            for i in range(n):
                writer.writerow([f"{t[i]:.6e}", f"{w[i]:.5g}"])
        print(f"wrote {path}")

    if args.plot or args.sim or args.read:
        import matplotlib.pyplot as plt

        if args.sim:
            # Align both traces at the turn-on edge (first rise through ALIGN), not the 2 V trigger: the
            # model's turn-on step stops short of 2 V, so a trigger alignment shifts it by a cycle.
            t0 = ltspice.rise_time(t, w, ALIGN)
            t = [x - t0 for x in t]
        plt.plot([x * 1e6 for x in t], w, lw=0.8, label=f"C{args.channel} (hardware)")
        if args.sim:
            st, sv = ltspice.aligned(args.sim, args.sim_node, ALIGN)
            keep = [i for i, x in enumerate(st) if t[0] <= x <= t[-1]]
            plt.plot([st[i] * 1e6 for i in keep], [sv[i] for i in keep], lw=0.8, ls=":", color="k",
                     label=f"{args.sim_node} (LTspice)")
            plt.legend()
        plt.axvline(0, color="k", lw=0.5, ls="--")
        plt.xlabel("time (us)")
        plt.ylabel("V")
        # plt.title(f"C{args.channel} start-up, trigger rising {LEVEL} V")
        plt.grid(alpha=0.3)
        plt.show()


if __name__ == "__main__":
    main()
