"""Solve max-cut problems on the device and report the cut vs the max cut.

    python run_maxcut.py ring4b ring8b cal_ring8b cube8b cal_cube8b bipart8b cal_bipart8b dense8b
    python run_maxcut.py ring8w cal_ring8w bipart8w dense8w --seed 3
    python run_maxcut.py cubic8b cubic8w --seed 3
    python run_maxcut.py er8b er8w --seed 3
    python run_maxcut.py star8b
    python run_maxcut.py ring4b --phase --write
    python run_maxcut.py k44w --publish       # uses DEVICE_TOKEN from the environment
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
K44 = [a + b for a in "ABCD" for b in "EFGH"]  # K4,4: ABCD vs EFGH
# Calibrated K4,4: the split minimising the largest cross gap (46.3 kHz; 58.1 kHz for K44).
CAL_K44 = ["".join(sorted(a + b)) for a in "ABFH" for b in "CDEG"]
STAR8 = ["A" + b for b in SPINS[1:]]  # hub A: unique max cut 7 is a 1 vs 7 split (A vs the rest)


def weighted(edges: list[str]):
    """edges with a uniform random weight each, 1..W_MAX so no edge is disconnected."""
    return lambda rng: {e: rng.randint(1, W_MAX) for e in edges}


def frustrated(edges: list[str]) -> bool:
    """Connected and non-bipartite on A..H: 2-colour from A; an odd cycle breaks the colouring, and spins
    left unreached mean the graph is disconnected."""
    side = {"A": 0}
    stack, odd = ["A"], False
    while stack:
        u = stack.pop()
        for e in edges:
            if u in e:
                v = e.replace(u, "", 1)
                if v not in side:
                    side[v] = 1 - side[u]
                    stack.append(v)
                elif side[v] == side[u]:
                    odd = True
    return odd and len(side) == len(SPINS)


def cubic8(rng: random.Random) -> list[str]:
    """Random connected, non-bipartite 3-regular graph on A..H: pair up three stubs per spin uniformly
    (configuration model) and redraw until simple. Non-bipartite so the max cut leaves an edge uncut."""
    while True:
        stubs = [s for s in SPINS for _ in range(3)]
        rng.shuffle(stubs)
        edges = sorted({"".join(sorted(stubs[k:k + 2])) for k in range(0, len(stubs), 2)})
        if len(edges) < len(stubs) // 2 or any(a == b for a, b in edges):
            continue  # self-loop or repeated edge
        if frustrated(edges):
            return edges


def er8(rng: random.Random, m: int = 12) -> list[str]:
    """Random Erdos-Renyi G(n, m) graph on A..H: m of the 28 edges uniformly, redrawn until connected and
    non-bipartite. m = 12 matches cubic8, so the two differ only in degree spread (cubic8: all 3)."""
    while True:
        edges = sorted(rng.sample([a + b for a, b in combinations(SPINS, 2)], m))
        if frustrated(edges):
            return edges


def dense8w(rng: random.Random) -> dict[str, int]:
    """Complete graph K8 with a uniform random weight per edge."""
    return {a + b: rng.randint(0, W_MAX) for a, b in combinations(SPINS, 2)}


# Problems: weight per edge. b = binary (unit weights), w = weighted; the random ones take an RNG.
GRAPHS = {
    "empty": dict.fromkeys((a + b for a, b in combinations(SPINS, 2)), 0),
    "ring4b": {"AB": 1, "BC": 1, "CD": 1, "DA": 1},
    "ring8b": dict.fromkeys(RING8, 1),
    "ring8w": weighted(RING8),
    "cal_ring8b": dict.fromkeys(CAL_RING8, 1),
    "cal_ring8w": weighted(CAL_RING8),
    "cube8b": dict.fromkeys(CUBE8, 1),
    "cube8w": weighted(CUBE8),
    "cal_cube8b": dict.fromkeys(CAL_CUBE8, 1),
    "cal_cube8w": weighted(CAL_CUBE8),
    "k44b": dict.fromkeys(K44, 1),
    "k44w": weighted(K44),
    "cal_k44b": dict.fromkeys(CAL_K44, 1),
    "cal_k44w": weighted(CAL_K44),
    "star8b": dict.fromkeys(STAR8, 1),
    "cubic8b": lambda rng: dict.fromkeys(cubic8(rng), 1),
    "cubic8w": lambda rng: weighted(cubic8(rng))(rng),  # same edges as cubic8b for a given seed
    "er8b": lambda rng: dict.fromkeys(er8(rng), 1),
    "er8w": lambda rng: weighted(er8(rng))(rng),  # same edges as er8b for a given seed
    "dense8b": dict.fromkeys((a + b for a, b in combinations(SPINS, 2)), 1),  # K8
    "dense8w": dense8w,
}


def phase_vs_a(r: dict) -> str:
    """Scope phase of B..D vs A (CH1..CH4), next to the phase the readout implies."""
    import phase
    import scope

    res = phase.measure(scope.HOST, [1, 2, 3, 4])
    line = f"\n{'':12} scope vs A:"
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
    p.add_argument("--phase", action="store_true", help="measure B..D phase vs A on scope CH1..CH4")
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
            line = (f"{name:12} {order} {state}  found {r['cut']} (max {r['max']}, {best}){f'  seed {seed}' if callable(graph) else ''}\n"
                    f"{'':12} ratio {r['cut'] / r['max'] if r['max'] else 1:.2f}  P(max) {r['p_max']:.3f}  corr "
                    + " ".join(f"{r['corr'][c]:+.2f}" for c in order))
            if args.phase:
                line += phase_vs_a(r)
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
