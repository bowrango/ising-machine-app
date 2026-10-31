"""Python interface to the ESP32 firmware's serial CLI, mirroring src/machine. Assumes the
oscillators are powered.

    python device.py "sample 10000"            # any CLI command
"""

import glob
import os
import select
import sys
import termios
import time
from itertools import product


R_W, R_AB = 75.0, 10000.0  # MCP4351-103 wiper and full-scale resistance (drivers/mcp4351.h)
CODE_MAX = 256

# Problem: weight per edge, e.g. {"AB": 1, "BC": 3}. State: phase per spin, e.g. {"A": "+", "B": "-"}.


def to_codes(problem: dict[str, float]) -> dict[str, int]:
    """Codes with conductance proportional to weight; the largest weight is code 0, zero weights are
    disconnected."""
    w_max = max(problem.values())
    return {e: min(CODE_MAX, round((w_max / w - 1) * R_W * 256 / R_AB)) for e, w in problem.items() if w > 0}


def cut_value(problem: dict[str, float], state: dict[str, str]) -> float:
    """Summed weight of edges between opposite phases."""
    return sum(w for (a, b), w in problem.items() if w and state[a] != state[b])


def max_cut(problem: dict[str, float], spins: str) -> tuple[float, dict[str, str]]:
    """(max cut, its state), by brute force."""
    states = (dict(zip(spins, "+" + "".join(st))) for st in product("+-", repeat=len(spins) - 1))
    return max(((cut_value(problem, st), st) for st in states), key=lambda c: c[0])


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
        """Read until `until` appears or timeout seconds pass with no data."""
        buf = b""
        last = time.time()
        while time.time() - last < timeout:
            if select.select([self.fd], [], [], 0.05)[0]:
                try:
                    buf += os.read(self.fd, 4096)
                except BlockingIOError:  # select can report a byte another reader then took
                    continue
                last = time.time()
                if until and until.encode() in buf:
                    break
        return buf.decode(errors="replace")

    def _command(self, cmd: str, until: str | None = None, timeout: float = 0.3) -> str:
        """Send cmd and return the reply, minus the echo."""
        os.write(self.fd, (cmd + "\n").encode())
        reply = self._read(timeout, until)
        return reply.split("\n", 1)[1] if reply.startswith(cmd) else reply

    def _call(self, cmd: str, lines: int = 0, timeout: float = 5.0) -> list[str]:
        """Send cmd and return the `lines` reply lines after its echo. Raises on an error reply."""
        termios.tcflush(self.fd, termios.TCIFLUSH)
        os.write(self.fd, (cmd + "\n").encode())
        buf = ""
        deadline = time.time() + timeout
        reply: list[str] = []
        while time.time() < deadline:
            buf += self._read(0.05)
            done = [line.strip() for line in buf.split("\n")[:-1]]  # complete lines only
            if cmd in done:
                reply = done[done.index(cmd) + 1:]
                if len(reply) >= lines or any(r.startswith(("Error", "Unknown command")) for r in reply):
                    break
        buf += self._read(0.05)  # an error can follow a reply that looked complete
        done = [line.strip() for line in buf.split("\n")]
        reply = [line for line in done[done.index(cmd) + 1:] if line] if cmd in done else reply
        errors = [line for line in reply if line.startswith(("Error", "Unknown command"))]
        if errors:
            raise RuntimeError(f"{cmd}: {errors[0]}")
        return reply

    # Couplers

    def write_binary_graph(self, edges: str):
        """Edges like "AB,BC" at code 0; the rest disconnected."""
        self._call(f"writeBinaryGraph {edges}")

    def write_weighted_graph(self, weights: dict[str, int]):
        """{edge: code}, e.g. {"AB": 0, "BC": 128}; the rest disconnected."""
        self._call("writeWeightedGraph " + ",".join(f"{e}:{c}" for e, c in weights.items()))

    def disconnect(self):
        self._call("disconnect")

    def read_graph(self) -> dict[str, int]:
        """Connected couplers as {edge: code}."""
        (line,) = self._call("readGraph", lines=1)
        return {} if line == "none" else {e: int(c) for e, c in (p.split(":") for p in line.split(","))}

    # Oscillators

    def sample(self, shots: int = 10000) -> dict[str, dict[str, float]]:
        """corr[i][j]: correlation of spins i and j's comparator bits as +-1 (2 * agreement - 1): +1 in
        phase, -1 antiphase, ~0 unlocked."""
        rows = self._call(f"sample {shots}", lines=8)
        return {r[0]: {s: 2 * float(p) / 100 - 1 for s, p in zip("ABCDEFGH", r.split()[1:])} for r in rows}

    # Max-cut

    def solve(self, problem: dict[str, float], shots: int = 10000) -> dict:
        """Program problem, sample, and score the majority phases (vs the first spin) against the max cut."""
        codes = to_codes(problem)
        spins = "".join(sorted(set("".join(codes))))
        self.disconnect()
        self.write_weighted_graph(codes)
        corr = self.sample(shots)[spins[0]]  # fix first spin as the reference
        state = {c: "+" if corr[c] >= 0 else "-" for c in spins}
        best, best_state = max_cut(problem, spins)
        return {"codes": codes, "state": state, "cut": cut_value(problem, state), "max": best,
                "best_state": best_state, "corr": corr}

    def close(self):
        os.close(self.fd)


if __name__ == "__main__":
    esp = ESP32()
    cmd = " ".join(sys.argv[1:])
    print(esp._command(cmd, until="=== End", timeout=2.0), end="")
    esp.close()
