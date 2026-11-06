# LTspice simulations

Simulations for [LTspice](https://www.analog.com/ltspice) (tested with 17.2.4 for macOS).

| File | Model |
|---|---|
| `oscillator.asc` | Schematic: start-up transient of one oscillator (cross-coupled SN74HC240 inverters through 1k, L + 750 pF tank (723p fitted to oscillator A), enabled by SW1 at 5 µs) |
| `oscillator.cir` | The same circuit as a netlist, for batch runs |
| `device4.asc` | Schematic: four oscillators A-D sharing the SYNC net and supply, with a digipot coupler between each pair at opposite terminals (default graph ring4b at code 0) |
| `device4.cir` | The same circuit as a netlist, for batch runs |
| `osc.lib` | Shared models: `hcinv` (HC240 inverter with input lag `td`), `osc` (one oscillator), `DCLAMP` |
| `hcinv.asy`, `osc.asy` | Symbols for the inverter and oscillator subcircuits |

Run the `.cir` in batch mode (`LTspice -b`), then `python3 ../io/transient.py 1 --sim` overlays `V(l)` on a scope capture of A_L.

Run `device4.cir` the same way, then `python3 ltspice.py device4.raw --phase a b c d` (from `hardware/io`) plots the phase of B, C and D against A as the ring settles. Each oscillator's `Ct` is fitted to its free-running frequency on the bench. Set a coupler's code to -1 to disconnect it; with every coupler disconnected, the oscillators are still coupled through the shared SYNC net.
