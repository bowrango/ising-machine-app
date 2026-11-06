"""Kuramoto model of an oscillator Ising machine (no noise): phases relax under the coupling Kc and
second-harmonic injection Ks (the SYNC signal at 2f), which pushes each phase to 0 or pi.

    dtheta_i/dt = Kc sum_j J_ij sin(theta_j - theta_i) - Ks sin(2 theta_i)

Time t is in oscillation cycles, so Kc and Ks are in rad/cycle. Kc is fixed; Ks ramps linearly from 0.

J is a Sherrington-Kirkpatrick instance, J_ij ~ N(0, 1/N), with Ising energy H(s) = -1/2 sum_ij J_ij s_i s_j.
Plotted per spin: E~(theta) = -1/2 sum_ij J_ij cos(theta_i - theta_j), its continuous relaxation;
H~(theta) = H(sign cos theta), the Ising energy of the rounded phases; and H~_min = -0.7633, the SK
ground-state energy per spin as N -> infinity (Parisi), so finite-N instances can land slightly above or below it.

    python kuramoto.py
    python kuramoto.py --n 200 --seed 3 --ks 0.2 --ramp 0.3
    python kuramoto.py --save kuramoto.png
"""

import argparse

import matplotlib.pyplot as plt
import numpy as np
from scipy.integrate import solve_ivp

BLUE = "#3B6A9E"
SK_EMIN = -0.7633  # Parisi ground-state energy per spin of the SK model


def wrap(theta: np.ndarray) -> np.ndarray:
    """Phases in [-pi, pi), NaN where a trace wraps so the plot doesn't draw a jump across the axis."""
    w = (theta + np.pi) % (2 * np.pi) - np.pi
    jump = np.abs(np.diff(w, axis=-1)) > np.pi
    w[..., 1:][jump] = np.nan
    return w


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--n", type=int, default=100, help="spins")
    p.add_argument("--seed", type=int, default=1)
    p.add_argument("--t", type=float, default=300.0, help="simulated time (cycles)")
    p.add_argument("--kc", type=float, default=0.1, help="coupling strength (rad/cycle)")
    p.add_argument("--ks", type=float, default=0.1, help="final SYNC (2nd-harmonic) strength (rad/cycle)")
    p.add_argument("--ramp", type=float, default=0.8, help="fraction of the run over which Ks ramps up from 0")
    p.add_argument("--save", help="write the figure here instead of showing it")
    args = p.parse_args()

    rng = np.random.default_rng(args.seed)
    n = args.n
    J = np.triu(rng.normal(0, 1 / np.sqrt(n), (n, n)), 1)
    J = J + J.T

    kc = lambda t: args.kc * np.ones_like(t)
    ks = lambda t: args.ks * np.clip(t / (args.ramp * args.t), 0, 1) if args.ramp > 0 else args.ks * np.ones_like(t)

    def rhs(t, th):
        return kc(t) * (J * np.sin(th[None, :] - th[:, None])).sum(1) - ks(t) * np.sin(2 * th)

    t = np.linspace(0, args.t, 2000)
    th0 = rng.uniform(-np.pi, np.pi, n)
    sol = solve_ivp(rhs, (0, args.t), th0, t_eval=t, rtol=1e-8, atol=1e-8)
    th = sol.y  # (n, time)

    c = np.cos(th)
    s = np.sin(th)
    E = -0.5 * (np.einsum("it,ij,jt->t", c, J, c) + np.einsum("it,ij,jt->t", s, J, s)) / n
    sg = np.where(c >= 0, 1.0, -1.0)
    H = -0.5 * np.einsum("it,ij,jt->t", sg, J, sg) / n
    Hmin = SK_EMIN
    print(f"N={n} seed={args.seed}: H~(end) = {H[-1]:.4f}, SK estimate {Hmin:.4f}")

    fig, (a1, a2, a3) = plt.subplots(3, 1, sharex=True, figsize=(4.8, 4.2), dpi=150,
                                     gridspec_kw={"height_ratios": [2, 1.6, 1], "hspace": 0})
    a1.plot(t, wrap(th).T, color="0.5", lw=0.7, alpha=0.8)
    a1.set_ylim(-np.pi, np.pi)
    a1.set_yticks([-np.pi, 0, np.pi], [r"$-\pi$", "0", r"$\pi$"])
    a1.set_ylabel(r"Phase $\theta_i$")

    a2.plot(t, E, color=BLUE, lw=1.6, label=r"$\widetilde{E}(\boldsymbol{\theta})$")
    a2.plot(t, H, color=BLUE, lw=1.6, ls=":", label=r"$\widetilde{H}(\boldsymbol{\theta})$")
    a2.axhline(Hmin, color="k", lw=1.2, label=r"$\widetilde{H}_\mathrm{min}$")
    a2.set_ylabel("Energy / $N$")
    a2.set_ylim(min(Hmin, E.min()) - 0.08, max(E[0], H[0]) + 0.05)
    a2.legend(frameon=False, fontsize=8, loc="upper right")

    a3.plot(t, kc(t), color="k", lw=1.4, label=r"$K_c$")
    a3.plot(t, ks(t), color=BLUE, lw=1.4, label=r"$K_s$")
    a3.set_ylim(0, 1.25 * max(args.kc, args.ks))
    a3.set_ylabel("Gain\n(rad/cycle)")
    a3.set_xlabel("Time (cycles)")
    a3.set_xlim(0, args.t)
    a3.legend(frameon=False, fontsize=8, loc="lower right", ncol=2)

    fig.align_ylabels()
    fig.tight_layout()
    if args.save:
        fig.savefig(args.save, dpi=300)
        print(f"wrote {args.save}")
    else:
        plt.show()


if __name__ == "__main__":
    main()
