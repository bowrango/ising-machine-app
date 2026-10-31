# Bench instruments

Python scripts that drive the bench instruments over raw SCPI sockets (port 5025), and the ESP32
through its serial CLI (`device.py`). `run_maxcut.py` solves max-cut problems through it; `phase.py`
measures frequency and relative phase on the scope.

## Network

- Scope: wired directly to the Mac's USB-C LAN adapter on a static private link, no router.
- macOS adds a new DHCP "USB 10/100/1000 LAN N" service each time the adapter appears on a new port.
- AWG: over the Mac's main network.
- Scope SCPI: rapid connect/disconnect locks up the server, and `SAST?` doesn't reply while a single trigger is armed (poll `TRMD?` instead).

Run `./setupbench.sh` to set the static IP on every USB LAN service and check the scope answers.
