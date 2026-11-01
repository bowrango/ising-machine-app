# LTspice simulations

Simulations for [LTspice](https://www.analog.com/ltspice) (tested with 17.2.4 for macOS).

| File | Model |
|---|---|
| `oscillator.asc` | Schematic: start-up transient of one oscillator (cross-coupled SN74HC240 inverters through 1k, L + 750 pF tank, enabled by SW1 at 5 µs) |
| `oscillator.cir` | The same circuit as a netlist, for batch runs |
| `hcinv.asy` | Inverter symbol for the schematic (subcircuit `hcinv`, defined in its directives) |