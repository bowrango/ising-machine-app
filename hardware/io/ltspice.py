"""Read LTspice binary .raw waveforms. Standard library only (+ matplotlib to plot).

    python ltspice.py device4.raw --phase a b c d             # phase of b, c, d vs a over time (device4.asc)

Bare .raw names are looked up in hardware/sim.
"""

import argparse
import bisect
import os
import struct

SIM_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "sim")  # hardware/sim
SIM_RAW = os.path.join(SIM_DIR, "oscillator.raw")

def read_raw(path: str) -> dict[str, list[float]]:
    """{variable: values} from a binary .raw; "time" is in seconds."""
    if not os.path.exists(path) and os.path.exists(os.path.join(SIM_DIR, path)):
        path = os.path.join(SIM_DIR, path)  # a bare name like device4.raw: look in hardware/sim
    data = open(path, "rb").read()
    marker = "Binary:\n".encode("utf-16-le")
    split = data.find(marker)
    assert split > 0, f"{path}: not a binary LTspice .raw"
    header = data[:split].decode("utf-16-le").splitlines()
    body = data[split + len(marker):]

    fields = {line.split(":", 1)[0]: line.split(":", 1)[1].strip() for line in header if ":" in line}
    nvars, npoints = int(fields["No. Variables"]), int(fields["No. Points"])
    names = [line.split("\t")[2] for line in header[header.index("Variables:") + 1:][:nvars]]
    # Time is a double; other variables are float32 unless the "double" flag is set.
    fmt = "<" + ("d" * nvars if "double" in fields["Flags"] else "d" + "f" * (nvars - 1))
    rows = list(struct.iter_unpack(fmt, body[:npoints * struct.calcsize(fmt)]))
    out = {name: [row[i] for row in rows] for i, name in enumerate(names)}
    out["time"] = [abs(t) for t in out["time"]]  # LTspice may flag points with a negative sign
    return out


def rise_time(t: list[float], v: list[float], level: float) -> float:
    """When v first rises through level (linearly interpolated)."""
    i = next(i for i in range(1, len(v)) if v[i - 1] < level <= v[i])
    return t[i - 1] + (level - v[i - 1]) / (v[i] - v[i - 1]) * (t[i] - t[i - 1])


def aligned(path: str, node: str, level: float) -> tuple[list[float], list[float]]:
    """(time, node voltage) with t = 0 where node first rises through level."""
    raw = read_raw(path)
    t, v = raw["time"], raw[node]
    t0 = rise_time(t, v, level)
    return [x - t0 for x in t], v


def rises(t: list[float], v: list[float], level: float = 0.0) -> list[float]:
    """Every time v rises through level (linearly interpolated)."""
    return [t[i - 1] + (level - v[i - 1]) / (v[i] - v[i - 1]) * (t[i] - t[i - 1])
            for i in range(1, len(v)) if v[i - 1] < level <= v[i]]


def phases(raw: dict[str, list[float]], osc: list[str]) -> tuple[list[float], dict[str, list[float]]]:
    """Phase (deg, + = leads) of each oscillator vs osc[0], once per cycle of osc[0], from rising zero
    crossings of the tank voltage V(x_l) - V(x_r). Same sign convention as phase.py."""
    t = raw["time"]
    up = {s: rises(t, [a - b for a, b in zip(raw[f"V({s}_l)"], raw[f"V({s}_r)"])]) for s in osc}
    ref = up[osc[0]]
    times, out = [], {s: [] for s in osc[1:]}
    for k in range(len(ref) - 1):
        period = ref[k + 1] - ref[k]
        nxt = {s: bisect.bisect_left(up[s], ref[k]) for s in osc[1:]}
        if any(j >= len(up[s]) for s, j in nxt.items()):
            break
        times.append(ref[k])
        for s, j in nxt.items():
            out[s].append(((ref[k] - up[s][j]) / period * 360 + 180) % 360 - 180)
    return times, out


def main():
    p = argparse.ArgumentParser()
    p.add_argument("raw")
    p.add_argument("--phase", nargs="+", metavar="OSC", required=True,
                   help="oscillators (net prefixes, e.g. a b c d): plot phase vs the first over time")
    args = p.parse_args()

    import matplotlib.pyplot as plt

    times, ph = phases(read_raw(args.raw), args.phase)
    tail = [i for i, x in enumerate(times) if x > times[-1] - 20e-6]  # last 20 us
    for s, deg in ph.items():
        print(f"{s} vs {args.phase[0]}: {sum(deg[i] for i in tail) / len(tail):+6.1f} deg (last 20 us)")
        plt.plot([x * 1e6 for x in times], deg, ".-", ms=2, lw=0.8, label=s.upper())
    plt.yticks(range(-180, 181, 45))
    plt.ylabel(f"phase vs {args.phase[0].upper()} (deg)")
    plt.xlabel("time (us)")
    plt.legend()
    plt.grid(alpha=0.3)
    plt.show()


if __name__ == "__main__":
    main()
