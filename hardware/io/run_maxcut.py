"""Solve max-cut problems on the device and report the cut vs the max cut.

    python run_maxcut.py ring4b ring8b cal_ring8b cube8b cal_cube8b bipart8b cal_bipart8b full8b
    python run_maxcut.py ring8w cal_ring8w bipart8w full8w --seed 3
    python run_maxcut.py ring4b --scope --write
    python run_maxcut.py bipart8w --publish       # uses DEVICE_TOKEN from the environment
"""

import argparse
import csv
import random
from datetime import datetime
from itertools import combinations
from pathlib import Path

from device import IsingMachine

OUT_DIR = Path(__file__).parent / "maxcut"
SPINS = "ABCDEFGH"
SCOPE_SPINS = "ABCD"  # A_L..D_L on scope CH1..CH4
W_MAX = 255  # largest random weight
RING8 = ["AB", "BC", "CD", "DE", "EF", "FG", "GH", "HA"]
# Calibrated order: neighbours' measured free-running frequencies within ~41 kHz, so it locks without SYNC.
CAL_RING8 = ["AD", "DC", "CE", "EH", "HB", "BG", "GF", "FA"]
# Cube: spins A..H are corners 000..111, joined when they differ in one bit. Bipartite: ADFG vs BCEH.
CUBE8 = [a + b for i, a in enumerate(SPINS) for j, b in enumerate(SPINS) if i < j and bin(i ^ j).count("1") == 1]
# Calibrated cube: oscillators placed on corners to minimise the largest neighbour frequency gap
# (43.7 kHz, G-H; 58.1 kHz for CUBE8). Bipartite: ADEG vs BCFH.
CAL_CUBE8 = ["AB", "AC", "AF", "BD", "BG", "CD", "CE", "DH", "EF", "EH", "FG", "GH"]
BIPART8 = [a + b for a in "ABCD" for b in "EFGH"]  # K4,4: ABCD vs EFGH
# Calibrated K4,4: the split minimising the largest cross gap (46.3 kHz; 58.1 kHz for BIPART8).
CAL_BIPART8 = ["".join(sorted(a + b)) for a in "ABFH" for b in "CDEG"]


def weighted(edges: list[str]):
    """edges with a uniform random weight each, 1..W_MAX so no edge is disconnected."""
    return lambda rng: {e: rng.randint(1, W_MAX) for e in edges}


def full8w(rng: random.Random) -> dict[str, int]:
    """Complete graph K8 with a uniform random weight per edge."""
    return {a + b: rng.randint(0, W_MAX) for a, b in combinations(SPINS, 2)}


# Problems: weight per edge. b = binary (unit weights), w = weighted; the random ones take an RNG.
GRAPHS = {
    "ring4b": {"AB": 1, "BC": 1, "CD": 1, "DA": 1},
    "ring8b": dict.fromkeys(RING8, 1),
    "ring8w": weighted(RING8),
    "cal_ring8b": dict.fromkeys(CAL_RING8, 1),
    "cal_ring8w": weighted(CAL_RING8),
    "cube8b": dict.fromkeys(CUBE8, 1),
    "cube8w": weighted(CUBE8),
    "cal_cube8b": dict.fromkeys(CAL_CUBE8, 1),
    "cal_cube8w": weighted(CAL_CUBE8),
    "bipart8b": dict.fromkeys(BIPART8, 1),
    "bipart8w": weighted(BIPART8),
    "cal_bipart8b": dict.fromkeys(CAL_BIPART8, 1),
    "cal_bipart8w": weighted(CAL_BIPART8),
    "full8b": dict.fromkeys((a + b for a, b in combinations(SPINS, 2)), 1),  # K8
    "full8w": full8w,
}


def scope_vs_a(r: dict) -> str:
    """Scope phase of B..D vs A (CH1..CH4), next to the phase the readout implies."""
    import phase
    import scope

    res = phase.measure(scope.HOST, [1, 2, 3, 4])
    line = "  scope vs A:"
    for ch, spin in enumerate(SCOPE_SPINS[1:], start=2):
        f, _, dphi = res[ch]
        r[f"df_{spin}"], r[f"phase_{spin}"] = f - res[1][0], dphi
        st = r["state"].get(spin) if "A" in r["state"] else None
        exp = "free" if st is None else ("0" if st == r["state"]["A"] else "180")
        line += f" {spin} {dphi:+4.0f} ({exp}, {r[f'df_{spin}'] / 1e3:+.1f}k)"
    return line


def main():
    p = argparse.ArgumentParser()
    p.add_argument("graphs", nargs="+", choices=GRAPHS)
    p.add_argument("--seed", type=int, help="seed for the random graphs (default: random)")
    p.add_argument("--shots", type=int, default=10000)
    p.add_argument("--scope", action="store_true")
    p.add_argument("--write", action="store_true")
    p.add_argument("--publish", action="store_true", help="send each result to isingmachine.app")
    args = p.parse_args()

    seed = args.seed if args.seed is not None else random.randrange(10000)
    machine = IsingMachine()
    rows = []
    try:
        for name in args.graphs:
            # Fresh RNG per graph so a seed always rebuilds the same graph.
            graph = GRAPHS[name]
            problem = graph(random.Random(seed)) if callable(graph) else graph
            order = "".join(sorted(set("".join(problem))))
            r = machine.solve(problem, args.shots)
            if args.publish:
                machine.publish(name, problem, r)
            state = "".join(r["state"][c] for c in order)
            best = "".join(r["best_state"][c] for c in order)
            label = f"{name} seed {seed}" if callable(graph) else name
            line = (f"{label:12} {order} {state}  found {r['cut']} (max {r['max']}, {best})\n"
                    f"{'':12} ratio {r['cut'] / r['max']:.2f}  corr "
                    + " ".join(f"{r['corr'][c]:+.2f}" for c in order))
            if args.scope:
                line += scope_vs_a(r)
            print(line)
            rows.append({**r, "graph": name, "seed": seed if callable(graph) else "", "order": order,
                         "state": state, "best_state": best, "problem": problem})
    finally:
        machine.disconnect()
        machine.close()

    if args.write:
        OUT_DIR.mkdir(exist_ok=True)
        path = OUT_DIR / f"maxcut_{datetime.now():%Y-%m-%d_%H%M%S}.csv"
        with open(path, "w", newline="") as f:
            w = csv.writer(f)
            scoped = [f"{k}_{s}" for s in SCOPE_SPINS[1:] for k in ("df", "phase")]
            w.writerow(["graph", "seed", "order", "state", "best_state", "found", "max", "problem",
                        "codes"] + scoped + [f"corr_{s}" for s in SPINS])
            for r in rows:
                w.writerow([r["graph"], r["seed"], r["order"], r["state"], r["best_state"], r["cut"], r["max"],
                            ",".join(f"{e}:{x}" for e, x in r["problem"].items()),
                            ",".join(f"{e}:{c}" for e, c in r["codes"].items())]
                           + [r.get(k, "") for k in scoped] + [f"{r['corr'][s]:.3f}" for s in SPINS])
        print(f"wrote {path}")


if __name__ == "__main__":
    main()
