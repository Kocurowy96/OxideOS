#!/usr/bin/env python3
# Port debugowy - etap 3: sterowanie mysza/klawiatura headless QEMU przez QMP `input-send-event`.
# OxideOS uzywa VMMouse w trybie absolutnym (patrz log bootu "PS/2 Mouse: VMMouse (Absolute)
# Enabled."), wiec ruch myszy jest zawsze wzgledem calego ekranu (0-0x7FFF na os), nie relatywny.
import argparse
import sys
import time

from qmp_client import QMPClient

DEFAULT_SCREEN = (1280, 720)
# OxideOS wykrywa klik przez proste probkowanie stanu przycisku w petli renderowania
# kompozytora (mouse_left && !prev_mouse_left) - bez przytrzymania miedzy "down" i "up"
# petla czasem nie zdazy zauwazyc przejscia stanu (zmierzone empirycznie 2026-09-20).
CLICK_HOLD_SECONDS = 0.15

# Mapa znak -> (qcode, czy_potrzebny_shift). Pelny zestaw znakow z US QWERTY, ktory kernel
# OxideOS faktycznie obsluguje od 2026-09-20 (Shift/Caps Lock w kernel/drivers/ps2_kbd.cpp) -
# bez polskich znakow diakrytycznych, tych kernel nie mapuje.
_CHAR_MAP = {' ': ('spc', False), '\n': ('ret', False), '\t': ('tab', False)}
for _c in 'abcdefghijklmnopqrstuvwxyz':
    _CHAR_MAP[_c] = (_c, False)
    _CHAR_MAP[_c.upper()] = (_c, True)
for _d in '0123456789':
    _CHAR_MAP[_d] = (_d, False)
for _sym, _digit_qcode in {'!': '1', '@': '2', '#': '3', '$': '4', '%': '5',
                           '^': '6', '&': '7', '*': '8', '(': '9', ')': '0'}.items():
    _CHAR_MAP[_sym] = (_digit_qcode, True)
_CHAR_MAP.update({
    '-': ('minus', False), '_': ('minus', True),
    '=': ('equal', False), '+': ('equal', True),
    '[': ('bracket_left', False), '{': ('bracket_left', True),
    ']': ('bracket_right', False), '}': ('bracket_right', True),
    ';': ('semicolon', False), ':': ('semicolon', True),
    "'": ('apostrophe', False), '"': ('apostrophe', True),
    '`': ('grave_accent', False), '~': ('grave_accent', True),
    '\\': ('backslash', False), '|': ('backslash', True),
    ',': ('comma', False), '<': ('comma', True),
    '.': ('dot', False), '>': ('dot', True),
    '/': ('slash', False), '?': ('slash', True),
})


def _key_event(qcode, down):
    return {"type": "key", "data": {"down": down, "key": {"type": "qcode", "data": qcode}}}


def press_key(client, qcode):
    client.call({"execute": "input-send-event", "arguments": {"events": [_key_event(qcode, True)]}})
    client.call({"execute": "input-send-event", "arguments": {"events": [_key_event(qcode, False)]}})


def type_text(client, text):
    for ch in text:
        mapped = _CHAR_MAP.get(ch)
        if mapped is None:
            print(f"Pomijam nieznany znak: {ch!r}", file=sys.stderr)
            continue
        qcode, shift = mapped
        if shift:
            client.call({"execute": "input-send-event", "arguments": {"events": [_key_event("shift", True)]}})
        press_key(client, qcode)
        if shift:
            client.call({"execute": "input-send-event", "arguments": {"events": [_key_event("shift", False)]}})


def move_abs(client, x, y, screen_w, screen_h):
    ax = int(x * 0x7FFF / max(screen_w - 1, 1))
    ay = int(y * 0x7FFF / max(screen_h - 1, 1))
    events = [
        {"type": "abs", "data": {"axis": "x", "value": ax}},
        {"type": "abs", "data": {"axis": "y", "value": ay}},
    ]
    client.call({"execute": "input-send-event", "arguments": {"events": events}})


def click(client, button):
    client.call({"execute": "input-send-event", "arguments": {"events": [{"type": "btn", "data": {"down": True, "button": button}}]}})
    time.sleep(CLICK_HOLD_SECONDS)
    client.call({"execute": "input-send-event", "arguments": {"events": [{"type": "btn", "data": {"down": False, "button": button}}]}})


def scroll(client, direction, amount):
    button = "wheel-up" if direction == "up" else "wheel-down"
    for _ in range(amount):
        click(client, button)


def main():
    parser = argparse.ArgumentParser(description="Steruje mysza/klawiatura headless QEMU przez QMP.")
    parser.add_argument("sock", help="sciezka do gniazda QMP")
    parser.add_argument("--screen", default=f"{DEFAULT_SCREEN[0]}x{DEFAULT_SCREEN[1]}", help="rozdzielczosc ekranu, np. 1280x720")
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_move = sub.add_parser("move", help="przesun kursor w pozycje X Y")
    p_move.add_argument("x", type=int)
    p_move.add_argument("y", type=int)

    p_click = sub.add_parser("click", help="klik myszy, opcjonalnie po przesunieciu w X Y")
    p_click.add_argument("--button", default="left", choices=["left", "right", "middle"])
    p_click.add_argument("--at", nargs=2, type=int, metavar=("X", "Y"))

    p_scroll = sub.add_parser("scroll", help="scroll kolko myszy")
    p_scroll.add_argument("direction", choices=["up", "down"])
    p_scroll.add_argument("--amount", type=int, default=1)

    p_key = sub.add_parser("key", help="pojedynczy klawisz (QEMU qcode, np. ret, esc, tab, a)")
    p_key.add_argument("qcode")

    p_type = sub.add_parser("type", help="wpisz tekst litera po literze")
    p_type.add_argument("text")

    args = parser.parse_args()
    screen_w, screen_h = (int(v) for v in args.screen.lower().split("x"))
    client = QMPClient(args.sock)

    if args.cmd == "move":
        move_abs(client, args.x, args.y, screen_w, screen_h)
    elif args.cmd == "click":
        if args.at:
            move_abs(client, args.at[0], args.at[1], screen_w, screen_h)
        click(client, args.button)
    elif args.cmd == "scroll":
        scroll(client, args.direction, args.amount)
    elif args.cmd == "key":
        press_key(client, args.qcode)
    elif args.cmd == "type":
        type_text(client, args.text)

    print("OK")


if __name__ == "__main__":
    main()
