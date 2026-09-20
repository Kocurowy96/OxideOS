#!/usr/bin/env python3
# Wspolny klient QMP - laczy sie do gniazda unix QEMU i robi handshake capabilities.
# Uzywane przez qmp_screendump.py i qmp_input.py, zeby nie duplikowac protokolu w dwoch miejscach.
import json
import socket


class QMPClient:
    def __init__(self, sock_path):
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.connect(sock_path)
        self.buf = b""
        self._read_line()  # QMP wysyla greeting jako pierwsza linie
        resp = self.call({"execute": "qmp_capabilities"})
        if "error" in resp:
            raise RuntimeError(f"Negocjacja QMP capabilities nie powiodla sie: {resp}")

    def _read_line(self):
        while b"\n" not in self.buf:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise ConnectionError("QMP socket closed unexpectedly")
            self.buf += chunk
        line, _, self.buf = self.buf.partition(b"\n")
        return json.loads(line)

    def call(self, cmd):
        self.sock.sendall((json.dumps(cmd) + "\n").encode())
        while True:
            resp = self._read_line()
            if "return" in resp or "error" in resp:
                return resp
            # inne linie to zdarzenia asynchroniczne (RESET itp.) - ignorujemy i czytamy dalej

    def close(self):
        self.sock.close()
