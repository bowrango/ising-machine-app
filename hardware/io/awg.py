"""Set a sine on a Siglent SDG AWG over a raw SCPI socket (port 5025). Standard library only.

    python awg.py 2e6 0.5            # 2 MHz, 0.5 Vpp into 50 ohm on CH1, output on
    python awg.py 2e6 0.5 --ch 2
    python awg.py --off              # CH1 output off
"""

import argparse
import socket

HOST = "192.168.55.110"
PORT = 5025


def main():
    p = argparse.ArgumentParser()
    p.add_argument("freq", type=float, nargs="?")
    p.add_argument("amp", type=float, nargs="?")
    p.add_argument("--ch", type=int, default=1)
    p.add_argument("--off", action="store_true")
    p.add_argument("--host", default=HOST)
    args = p.parse_args()
    assert args.off or (args.freq is not None and args.amp is not None), "freq and amp are required unless --off"

    with socket.create_connection((args.host, PORT), timeout=5) as s:
        f = s.makefile("rw", newline="\n")

        def send(cmd):
            f.write(cmd + "\n")
            f.flush()

        send(f"C{args.ch}:OUTP OFF,LOAD,50")
        if not args.off:
            send(f"C{args.ch}:BSWV WVTP,SINE,FRQ,{args.freq},AMP,{args.amp},OFST,0")
            send(f"C{args.ch}:OUTP ON,LOAD,50")
        send(f"C{args.ch}:BSWV?")
        print(f.readline().strip())


if __name__ == "__main__":
    main()
