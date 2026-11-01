# ising-machine-app

An 8-spin oscillator Ising machine that solves max-cut problems in hardware. Results are published to [isingmachine.app](https://isingmachine.app).

```
Mac (hardware/io) ──serial──▶ ESP32 ──SPI──▶ couplers + comparators
        └──POST /api/result──▶ isingmachine.app
```

## Solving max-cut

[`hardware/io/device.py`](hardware/io/device.py) is the Python interface to the device. A problem is a weight per edge between spins `A`–`H`; `solve` programs it on the couplers, samples the oscillator phases, and scores the read-out against the brute-force max cut.
```python
from device import IsingMachine

machine = IsingMachine()                        # open device serial port
problem = {"AB": 1, "BC": 1, "CD": 1, "DA": 1}  # define problem
r = machine.solve(problem)
machine.publish("ring4b", problem, r)           # POST result to isingmachine.app
machine.close()
```

[`run_maxcut.py`](hardware/io/run_maxcut.py) runs preset problems (`ring4b`, `ring8b`/`ring8w`, `cal_ring8b`/`cal_ring8w`, `cube8b`/`cube8w`, `cal_cube8b`/`cal_cube8w`, `bipart8b`/`bipart8w`, `cal_bipart8b`/`cal_bipart8w`, `full8b`/`full8w`):
```
python3 hardware/io/run_maxcut.py cal_ring8b bipart8w --publish
```

## Layout

```
hardware/    schematics and bench scripts (hardware/io: device.py, run_maxcut.py)
firmware/    ESP32 sketch exposing the serial CLI that device.py drives (firmware/README.md)
web/         isingmachine.app on Cloudflare (web/README.md)
```
