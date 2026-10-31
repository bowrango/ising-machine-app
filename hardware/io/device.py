"""Python interface to the ESP32 firmware's serial CLI. Standard library only (no pyserial).

    python device.py "sample 10000"            # any CLI command
    python device.py "writeBinaryGraph AB,BC,CD,DA"

The ESP32 methods mirror the firmware's machine layer (src/machine): couplers (write_binary_graph,
write_weighted_graph, read_graph, disconnect) and oscillators (sample). The oscillators are assumed powered (SW1 on)
whenever an ESP32 is created. Only one program can hold the port at a time.
"""

import glob
import os
import select
import sys
import termios
import time


def find_port() -> str:
    ports = glob.glob("/dev/cu.usbserial-*")
    assert ports, "no /dev/cu.usbserial-* port: is the Feather plugged in?"
    return ports[0]


class ESP32:
    def __init__(self, port: str | None = None):
        self.fd = os.open(port or find_port(), os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        a = termios.tcgetattr(self.fd)
        a[0] = a[1] = a[3] = 0  # raw: no input, output or local processing
        a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
        a[4] = a[5] = termios.B115200
        termios.tcsetattr(self.fd, termios.TCSANOW, a)
        self._read(1.0)  # drop any boot banner or stale output

    def _read(self, timeout: float, until: str | None = None) -> str:
        """Read until `until` appears, or until timeout seconds pass with no new data."""
        buf = b""
        last = time.time()
        while time.time() - last < timeout:
            if select.select([self.fd], [], [], 0.05)[0]:
                buf += os.read(self.fd, 4096)
                last = time.time()
                if until and until.encode() in buf:
                    break
        return buf.decode(errors="replace")

    def _command(self, cmd: str, until: str | None = None, timeout: float = 0.3) -> str:
        """Send cmd and return the firmware's reply (the echoed command line is dropped)."""
        os.write(self.fd, (cmd + "\n").encode())
        reply = self._read(timeout, until)
        return reply.split("\n", 1)[1] if reply.startswith(cmd) else reply

    def _call(self, cmd: str, lines: int = 0, timeout: float = 5.0) -> list[str]:
        """Send cmd, wait for its echo plus `lines` reply lines, and return those lines. Raises on an
        "Error:" or "Unknown command" reply."""
        os.write(self.fd, (cmd + "\n").encode())
        buf = ""
        deadline = time.time() + timeout
        while buf.count("\n") < lines + 1 and "Error" not in buf and "Unknown" not in buf and time.time() < deadline:
            buf += self._read(0.05)
        if "Error" not in buf:
            buf += self._read(0.05)  # an error can follow the echo
        reply = buf.strip().splitlines()[1:]
        errors = [line for line in reply if line.startswith(("Error", "Unknown command"))]
        if errors:
            raise RuntimeError(f"{cmd}: {errors[0]}")
        return reply

    # Couplers

    def write_binary_graph(self, edges: str):
        """Edges (e.g. "AB,BC") at code 0, max coupling; every other coupler disconnected."""
        self._call(f"writeBinaryGraph {edges}")

    def write_weighted_graph(self, weights: dict[str, int]):
        """Each edge at its code (0..256, lower = stronger coupling), e.g. {"AB": 0, "BC": 128}; every
        other coupler disconnected."""
        self._call("writeWeightedGraph " + ",".join(f"{e}:{c}" for e, c in weights.items()))

    def disconnect(self):
        self._call("disconnect")

    def read_graph(self) -> dict[str, int]:
        """Connected couplers, read back from the digipots, as {edge: code}, e.g. {"AB": 0, "BC": 0}."""
        (line,) = self._call("readGraph", lines=1)
        return {} if line == "none" else {e: int(c) for e, c in (p.split(":") for p in line.split(","))}

    # Oscillators

    def sample(self, shots: int = 10000) -> dict[str, dict[str, float]]:
        """agree[i][j]: % of shots spins i and j had the same comparator bit (100 in phase, 0 antiphase,
        ~50 unlocked)."""
        rows = self._call(f"sample {shots}", lines=8)
        return {r[0]: dict(zip("ABCDEFGH", map(float, r.split()[1:]))) for r in rows}

    def close(self):
        os.close(self.fd)


if __name__ == "__main__":
    esp = ESP32()
    cmd = " ".join(sys.argv[1:])
    print(esp._command(cmd, until="=== End", timeout=2.0), end="")
    esp.close()
