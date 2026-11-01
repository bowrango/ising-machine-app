"""Set a sine on a Siglent SDG AWG over a raw SCPI socket (port 5025). Standard library only.

    python awg.py 2e6 0.5            # 2 MHz, 0.5 Vpp into 50 ohm on CH1, output on
    python awg.py 2e6 0.5 --ch 2
    python awg.py --off              # CH1 output off
"""

import argparse
import socket

HOST = "192.168.55.110"
PORT = 5025


def command(host: str, cmds: list[str]) -> str:
    """Send cmds, then return the channel's BSWV? reply (the last command must be that query)."""
    with socket.create_connection((host, PORT), timeout=5) as s:
        f = s.makefile("rw", newline="\n")
        for cmd in cmds:
            f.write(cmd + "\n")
        f.flush()
        return f.readline().strip()


def sine(freq: float, amp: float, ch: int = 1, host: str = HOST) -> str:
    """Set a freq Hz, amp Vpp (into 50 ohm) sine and turn the output on. Changes the waveform without
    toggling the output, so a running sweep doesn't glitch the load."""
    return command(host, [
        f"C{ch}:BSWV WVTP,SINE,FRQ,{freq},AMP,{amp},OFST,0",
        f"C{ch}:OUTP ON,LOAD,50",
        f"C{ch}:BSWV?",
    ])


def off(ch: int = 1, host: str = HOST) -> str:
    return command(host, [f"C{ch}:OUTP OFF,LOAD,50", f"C{ch}:BSWV?"])


def main():
    p = argparse.ArgumentParser()
    p.add_argument("freq", type=float, nargs="?")
    p.add_argument("amp", type=float, nargs="?")
    p.add_argument("--ch", type=int, default=1)
    p.add_argument("--off", action="store_true")
    p.add_argument("--host", default=HOST)
    args = p.parse_args()
    assert args.off or (args.freq is not None and args.amp is not None), "freq and amp are required unless --off"

    if args.off:
        print(off(args.ch, args.host))
    else:
        print(sine(args.freq, args.amp, args.ch, args.host))


if __name__ == "__main__":
    main()
