"""Solve max-cut problems on the device and report the cut vs the max cut.

    python run_maxcut.py ring4b ring8b bipart8b
    python run_maxcut.py bipart8w rand8b rand8w --seed 3
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


def rand8b(rng: random.Random) -> dict[str, int]:
    """Each pair of spins an edge with probability 1/2."""
    return {a + b: 1 for a, b in combinations(SPINS, 2) if rng.random() < 0.5}


def bipart8w(rng: random.Random) -> dict[str, int]:
    """K4,4 (ABCD vs EFGH) with a uniform random weight per edge."""
    return {a + b: rng.randint(0, W_MAX) for a in "ABCD" for b in "EFGH"}


def rand8w(rng: random.Random) -> dict[str, int]:
    """Every pair of spins an edge with a uniform random weight."""
    return {a + b: rng.randint(0, W_MAX) for a, b in combinations(SPINS, 2)}


# Problems: weight per edge. b = binary (unit weights), w = weighted; the random ones take an RNG.
GRAPHS = {
    "ring4b": {"AB": 1, "BC": 1, "CD": 1, "DA": 1},
    # Order keeps neighbours within ~41 kHz of each other so the ring locks without SYNC.
    "ring8b": {"AD": 1, "DC": 1, "CE": 1, "EH": 1, "HB": 1, "BG": 1, "GF": 1, "FA": 1},
    "bipart8b": {a + b: 1 for a in "ABCD" for b in "EFGH"},  # K4,4: ABCD vs EFGH
    "bipart8w": bipart8w,
    "rand8b": rand8b,
    "rand8w": rand8w,
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
