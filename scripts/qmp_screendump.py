#!/usr/bin/env python3
# Port debugowy - etap 1: zrzut ekranu emulatora przez QMP `screendump` (zapisuje PPM).
import sys

from qmp_client import QMPClient


def main():
    if len(sys.argv) != 3:
        print("Uzycie: qmp_screendump.py <sciezka do gniazda QMP> <sciezka wyjsciowa .ppm>", file=sys.stderr)
        sys.exit(1)

    sock_path, out_path = sys.argv[1], sys.argv[2]
    client = QMPClient(sock_path)

    resp = client.call({"execute": "screendump", "arguments": {"filename": out_path}})
    if "error" in resp:
        print(f"screendump nie powiodl sie: {resp}", file=sys.stderr)
        sys.exit(1)

    print(f"Zrzut ekranu zapisany: {out_path}")


if __name__ == "__main__":
    main()
