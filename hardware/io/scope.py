"""Read waveforms from a Siglent SDS1000X-U/X-E scope over a raw SCPI socket (port 5025).

    python scope.py 1              # CH1: print Vpp / mean / RMS
    python scope.py 1 2 --write    # CH1 + CH2, also save time,C1,C2 to scope_<datetime>.csv
    python scope.py 1 --plot       # CH1, also plot the capture in a window

The scope is wired directly to the Mac: scope LAN static 192.168.10.2/24, Mac USB-C adapter 192.168.10.1/24.
Reads what's on screen; press Stop on the scope first for a frozen capture.
"""

import argparse
import csv
import math
import re
import socket
from datetime import datetime

# Configured Mac: System Settings > Network > (the USB-C LAN adapter) > Details > TCP/IP. Configure IPv4 manually as 192.168.10.1, mask 255.255.255.0, router blank
# Configured Siglent: Utility > I/O > IP Set. DHCP OFF; IP 192.168.10.2; mask 255.255.255.0; gateway 192.168.10.1.
HOST = "192.168.10.2"
PORT = 5025
GRID = 14  # horizontal divisions


def number(reply: str) -> float:
    """'1.00E-01V' or 'SARA 2.50E+08Sa/s' -> float."""
    return float(re.search(r"[-+]?\d+(\.\d*)?([eE][-+]?\d+)?", reply.split(" ")[-1]).group())


def read(host: str, channels: list[int]):
    """Return (time, {ch: volts}, sample_rate) for the waveforms in acquisition memory."""
    with socket.create_connection((host, PORT), timeout=10) as s:
        f = s.makefile("rwb")

        def send(cmd):
            f.write(cmd.encode() + b"\n")
            f.flush()

        def query(cmd):
            send(cmd)
            return f.readline().decode().strip()

        send("CHDR OFF")
        tdiv = number(query("TDIV?"))
        delay = number(query("TRDL?"))
        srate = number(query("SARA?"))

        waves = {}
        for ch in channels:
            vdiv = number(query(f"C{ch}:VDIV?"))
            offset = number(query(f"C{ch}:OFST?"))
            send(f"C{ch}:WF? DAT2")
            # Reply: DAT2,#9<9-digit length><int8 samples>\n\n
            while f.read(1) != b"#":
                pass
            ndigits = int(f.read(1))
            raw = f.read(int(f.read(ndigits)))
            f.read(2)
            waves[ch] = [(b - 256 if b > 127 else b) * vdiv / 25 - offset for b in raw]

    n = min(len(w) for w in waves.values())
    t0 = -delay - tdiv * GRID / 2
    time = [t0 + i / srate for i in range(n)]
    return time, {ch: w[:n] for ch, w in waves.items()}, srate


def main():
    p = argparse.ArgumentParser()
    p.add_argument("channels", type=int, nargs="+", choices=[1, 2, 3, 4])
    p.add_argument("--host", default=HOST)
    p.add_argument("--write", action="store_true")
    p.add_argument("--plot", action="store_true")
    args = p.parse_args()

    time, waves, srate = read(args.host, args.channels)
    n = len(time)

    print(f"{n} samples at {srate:.3g} Sa/s")
    for ch, w in waves.items():
        mean = sum(w) / len(w)
        rms = math.sqrt(sum(v * v for v in w) / len(w))
        print(f"C{ch}: Vpp {max(w) - min(w):.4g} V  mean {mean:.4g} V  RMS {rms:.4g} V")

    if args.write:
        path = f"scope_{datetime.now():%Y-%m-%d_%H%M%S}.csv"
        with open(path, "w", newline="") as out:
            w = csv.writer(out)
            w.writerow(["time"] + [f"C{ch}" for ch in waves])
            for i in range(n):
                w.writerow([f"{time[i]:.6e}"] + [f"{waves[ch][i]:.5g}" for ch in waves])
        print(f"wrote {path}")

    if args.plot:
        import matplotlib.pyplot as plt

        for ch, w in waves.items():
            plt.plot([t * 1e6 for t in time], w, lw=0.8, label=f"C{ch}")
        plt.xlabel("time (us)")
        plt.ylabel("V")
        plt.title(f"{n} samples at {srate:.3g} Sa/s")
        plt.legend()
        plt.grid(alpha=0.3)
        plt.show()


if __name__ == "__main__":
    main()
