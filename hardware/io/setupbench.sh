#!/bin/bash
# Point the Mac's USB-C LAN adapter at the scope (see scope.py), then check the scope answers.
#
#     ./setupbench.sh
#
# macOS adds a new "USB 10/100/1000 LAN N" network service each time the adapter appears on a new
# port, so the static IP goes on all of them; only the one that's plugged in is active.

MAC_IP=192.168.10.1
MASK=255.255.255.0
SCOPE_IP=192.168.10.2
SCOPE_PORT=5025

networksetup -listallnetworkservices | grep '^USB 10/100/1000 LAN' | while read -r service; do
  networksetup -setmanual "$service" "$MAC_IP" "$MASK" && echo "$service: $MAC_IP"
done

sleep 2
if ping -c 2 -t 3 "$SCOPE_IP" > /dev/null && nc -z -G 3 "$SCOPE_IP" "$SCOPE_PORT"; then
  echo "Scope reachable at $SCOPE_IP:$SCOPE_PORT"
else
  echo "Scope not reachable at $SCOPE_IP:$SCOPE_PORT (check cable and scope Utility > I/O > IP Set)"
  exit 1
fi
