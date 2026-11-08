"""Plot the max-cut graph presets from run_maxcut.py. Spins sit on a circle in A..H order; node fill is the
max-cut partition (white vs black, A white). Edge width and blue opacity scale
with weight, matching the web result matrix (weights are labelled on weighted graphs).
Spins not in the problem are grey.

    python plot_graph.py ring8b cal_ring8b cube8b
    python plot_graph.py cubic8b cubic8w --seed 3
    python plot_graph.py --all --save graphs.png
"""

import argparse
import math
import random

import matplotlib.pyplot as plt

from device import max_cut
from run_maxcut import GRAPHS, SPINS


COUPLING_RGB = (37, 99, 235)  # --coupling in web/src/index.css


def coupling(w: float, w_max: float) -> tuple[float, float, float, float]:
    """Edge colour as on the web result matrix: rgba(--coupling, 0.15 + 0.6 * w / w_max)."""
    return (*(c / 255 for c in COUPLING_RGB), 0.15 + 0.6 * w / w_max)


def problem_of(name: str, seed: int) -> dict[str, int]:
    """The preset's edge weights; random presets are rebuilt from seed exactly as run_maxcut does."""
    graph = GRAPHS[name]
    return graph(random.Random(seed)) if callable(graph) else graph


def draw(ax, name: str, problem: dict[str, int], seed: int | None = None):
    edges = {e: w for e, w in problem.items() if w}
    spins = "".join(sorted(set("".join(edges)))) or SPINS
    best, state = max_cut(edges, spins) if edges else (0, dict.fromkeys(spins, "+"))

    pos = {s: (math.sin(2 * math.pi * k / len(SPINS)), math.cos(2 * math.pi * k / len(SPINS)))
           for k, s in enumerate(SPINS)}
    w_max = max(edges.values(), default=1)
    binary = all(w == 1 for w in edges.values())

    for (a, b), w in edges.items():
        (x0, y0), (x1, y1) = pos[a], pos[b]
        ax.plot([x0, x1], [y0, y1], color=coupling(w, w_max), lw=0.6 + 2.4 * w / w_max, zorder=1)
        if not binary:
            ax.text((x0 + x1) / 2, (y0 + y1) / 2, str(w), fontsize=6, ha="center", va="center", zorder=2,
                    bbox=dict(boxstyle="round,pad=0.1", fc="white", ec="none"))
    for s in SPINS:
        x, y = pos[s]
        dark = state.get(s) == "-"
        edge = "k" if s in state else "0.75"  # spins outside the problem are greyed out
        ax.scatter([x], [y], s=420, c="k" if dark else "white", edgecolors=edge, linewidths=1.2, zorder=3)
        ax.text(x, y, s, ha="center", va="center", fontsize=10, color="white" if dark else edge, zorder=4)

    title = f"{name}{f'  (seed {seed})' if seed is not None else ''}\nmax cut {best}"
    ax.set_title(title, fontsize=10)
    ax.set_xlim(-1.3, 1.3)
    ax.set_ylim(-1.3, 1.3)
    ax.set_aspect("equal")
    ax.axis("off")


def main():
    p = argparse.ArgumentParser()
    p.add_argument("graphs", nargs="*", choices=GRAPHS, help="presets to plot")
    p.add_argument("--all", action="store_true", help="plot every preset")
    p.add_argument("--seed", type=int, help="seed for the random graphs (default: random)")
    p.add_argument("--cols", type=int, default=4)
    p.add_argument("--save", help="write the figure here instead of showing it")
    args = p.parse_args()

    names = list(GRAPHS) if args.all else args.graphs
    if not names:
        p.error("name at least one preset, or pass --all")
    seed = args.seed if args.seed is not None else random.randrange(10000)

    cols = min(args.cols, len(names))
    rows = math.ceil(len(names) / cols)
    fig, axes = plt.subplots(rows, cols, figsize=(2.8 * cols, 3.0 * rows), squeeze=False)
    for ax in axes.flat[len(names):]:
        ax.axis("off")
    for ax, name in zip(axes.flat, names):
        draw(ax, name, problem_of(name, seed), seed if callable(GRAPHS[name]) else None)
    fig.tight_layout()
    if args.save:
        fig.savefig(args.save, dpi=300, bbox_inches="tight")
        print(f"wrote {args.save}")
    else:
        plt.show()


if __name__ == "__main__":
    main()
