# Networking

The network stack is built from the hardware register level up, in layers that call
directly into each other (no callback/registration abstraction between layers — the same
direct-call style as `VFS::` calling `Ext2::`):

```
kernel/drivers/rtl8139.cpp   (hardware: PCI device, RX/TX rings, interrupts)
            |
            v
kernel/net/ethernet.cpp      (frame build/parse, dispatch by EtherType)
            |
            +--> kernel/net/arp.cpp   (address resolution)
            |
            +--> kernel/net/ip.cpp    (IPv4 build/parse, dispatch by protocol)
                        |
                        +--> kernel/net/icmp.cpp   (echo request/reply)
                        |
                        +--> kernel/net/udp.cpp    (datagrams, dispatch by dest port)
                        |           |
                        |           +--> kernel/net/dhcp.cpp   (client: DISCOVER/OFFER/REQUEST/ACK)
                        |
                        +--> kernel/net/tcp.cpp    (single-connection client: handshake/data/close)
                                    |
                                    +--> kernel/net/http.cpp   (minimal HTTP/1.0 GET on top of tcp.cpp)
```

`kernel/net/config.h` holds `NET_OUR_IP` and `NET_GATEWAY_IP` — **no longer hardcoded**
(they were `10.0.2.15`/`10.0.2.2` as `static const` through the ICMP milestone). Since
the DHCP client landed, both are mutable `extern uint8_t[4]` (defined in `config.cpp`),
starting as `{0,0,0,0}` and filled in by `DHCP::Run()` (called once at boot, immediately
after `RTL8139::Init()`, before anything else in the network stack touches them).

## RTL8139 driver (`kernel/drivers/rtl8139.*`)

- Detected via `PCI::FindDevice(0x10EC, 0x8139, ...)`; BAR0 is an I/O-port range (not
  memory-mapped).
- RX: an 8 KiB ring buffer (`RBSTART` = physical address, card DMAs directly into it), with
  a 4-byte header (status + length, length includes the trailing 4-byte CRC) in front of
  every received frame. `CAPR` (the "consumer" read pointer the card watches) has a
  well-known hardware quirk: it must be written as *(actual read offset − 16)*, not the
  offset itself — documented on the OSDev wiki, easy to get wrong.
- TX: 4 descriptor/buffer pairs (`TSAD0-3`/`TSD0-3`), rotated round-robin
  (`tx_next_desc`). **This rotation is required, not an optimization** — an earlier version
  always used descriptor 0 and worked for exactly one transmission before every subsequent
  send on that descriptor timed out waiting for the `TOK` (transmit-OK) bit. Reusing a
  descriptor immediately after its own completion isn't safe on this hardware/emulation;
  rotating across all 4 sidesteps it.
- The IRQ line is **not** a fixed number like the keyboard (1) or mouse (12) — PCI assigns
  it dynamically, read from PCI config space offset `0x3C` (`RTL8139::GetIrqLine()`) and
  used directly in `kernel/cpu/isr.cpp`'s dispatch instead of a hardcoded constant.

## Ethernet / ARP / IP / ICMP

- **Byte order**: every multi-byte protocol field (EtherType, ARP `htype`/`ptype`/`oper`,
  IP `total_length`/`id`/`checksum`, ICMP `checksum`/`id`/`seq`) is big-endian on the wire;
  x86_64 is little-endian. Each file has its own small `Swap16()` helper (duplicated per
  file, matching the project's convention of no shared cross-file micro-utilities) — easy
  detail to silently get backwards.
- **Checksums**: IP and ICMP both use the RFC 1071 Internet checksum (one's-complement sum
  of 16-bit big-endian words over the raw wire bytes with the checksum field itself zeroed,
  carries folded, then complemented) — implemented locally in each of `ip.cpp`/`icmp.cpp`.
- **ARP** (`kernel/net/arp.cpp`): an 8-entry static IP↔MAC table, populated by "gratuitous
  learning" from *every* received ARP packet regardless of type, plus real
  request/reply handling (`SendRequest`, `HandleFrame` replies to requests for our own IP).
  `ARP::Resolve` does a bounded, blocking wait (with `SendRequest` if the entry is unknown)
  — there is no packet queueing while waiting for a resolution.
- **IP** (`kernel/net/ip.cpp`): builds/parses IPv4 headers, no fragmentation support.
  `IP::Send` resolves the destination MAC via ARP as above before handing the frame to
  `Ethernet::Send`.
- **ICMP** (`kernel/net/icmp.cpp`): `SendEchoRequest`/`HandleFrame` — OxideOS can both send
  pings and reply to pings sent to it.

## UDP, DHCP, TCP, HTTP

- **UDP** (`kernel/net/udp.cpp`): `Send`/`HandleFrame`, same RFC 1071 checksum but over a
  pseudo-header (src/dst IP + protocol + length) *and* the segment — the two buffers never
  sit next to each other in memory, so the checksum helper is split into
  `AccumulateSum`/`FinishChecksum` (two calls over two separate buffers) instead of the
  single-buffer `ComputeChecksum` that IP/ICMP use. `HandleFrame` dispatches by destination
  port: port 7 runs a plain RFC 862 echo service, port 68 hands the packet to
  `DHCP::HandleFrame`.
- **DHCP** (`kernel/net/dhcp.cpp`): a client only — `DHCP::Run()` blocks through the full
  DISCOVER→OFFER→REQUEST→ACK exchange once at boot and fills in `NET_OUR_IP`/
  `NET_GATEWAY_IP` from the ACK (`yiaddr`, option 3 "Router"). No lease renewal — deliberately
  out of scope, same kind of accepted simplification as `critical.h`. Needed a real, if
  small, change to already-working code: `IP::Send` couldn't send to `255.255.255.255`
  before this (it always ARP-resolved the destination first), so it gained an early check
  that sends straight to the Ethernet broadcast address for that one destination IP,
  skipping ARP — every existing caller (ICMP, UDP) always passed a real unicast address,
  so this changes nothing for them. The DHCP state machine gates on an explicit "what am I
  waiting for" flag (`OFFER` vs `ACK`), not a single shared status flag — a stray duplicate
  `OFFER` arriving while waiting for the `ACK` would otherwise be mistaken for the reply.
- **TCP** (`kernel/net/tcp.cpp`): client-only, one connection at a time, blocking
  `Connect`/`SendData`/`Receive`/`Close` — no listening/accept, no real congestion control
  or window, no retransmission (the plan accepts this for a "clean" virtual link with no
  real packet loss). State machine: `CLOSED → SYN_SENT → ESTABLISHED → (PEER_CLOSED if the
  other side closes first) → CLOSING → CLOSED`. The receive buffer is the one piece of
  networking state that needs more than `volatile` — it's a multi-byte copy shared between
  the RX interrupt path and the blocking `Receive()` call, so it's wrapped in
  `EnterCritical`/`ExitCritical` (`kernel/cpu/critical.h`) instead.
- **HTTP** (`kernel/net/http.cpp`): `HTTP::Get()` — one `GET` request over `TCP::`, reads
  until the connection closes (`TCP::Receive` returning 0), no `Content-Length` parsing, no
  chunked encoding, no TLS, no keep-alive. Intentionally the smallest thing that can fetch a
  real page from a real server.

There's no syscall/application API for any of this yet — everything above is exercised by
temporary calls added to `kernel/main.cpp` for verification and removed afterward (the same
pattern used for the early Ext2 diagnostics). Wiring it up for userspace apps is its own
future phase.

## QEMU networking model — what "works" actually means

`-netdev user` (SLIRP) gives the guest NAT-style access to a **private** `10.0.2.0/24`
subnet (guest = `10.0.2.15` by default, gateway = `10.0.2.2`) that is **not reachable from
the real host** — `ping 10.0.2.15` from the host returns "Destination Net Unreachable";
only guest-initiated traffic actually crosses that boundary, or one of two explicit
forwarding flags on `-netdev user`, each for a different direction:

- `hostfwd=tcp/udp::PORT-:GUESTPORT` — **host connects to guest**: a connection made to
  `PORT` on the host is forwarded to `GUESTPORT` inside the guest. Used to verify the UDP
  echo server (Faza 5) — `nc -u localhost <PORT>` from the host reaches OxideOS's own
  listener.
- `guestfwd=tcp:ADDR:PORT-cmd:COMMAND` — **guest connects out to the host**: a guest
  connection to `ADDR:PORT` is handed to a process SLIRP spawns on the host (its stdin/
  stdout wired directly to the TCP stream), letting the guest reach a real host-side
  service through a thin bridge command (e.g. `nc 127.0.0.1 <realport>` proxying to an
  already-running host listener). Used to verify the TCP client and HTTP client (Fazy
  6b/6c) — connecting to a real `nc`/`python3 -m http.server` process actually listening on
  the host. Easy to reach for `hostfwd` here by habit — it's the wrong direction and simply
  doesn't work for "OxideOS connects out"; this was caught before writing the test, not
  after a failed one.

This is also why the network milestone is framed as *"OxideOS pings out and gets a real
reply"*, not *"the host can ping OxideOS"* — the latter was the original plan and turned
out to be impossible with this QEMU networking mode; corrected once discovered empirically.

## Independent verification

Every phase of this stack was checked against tools that don't trust the kernel's own
account of what happened:

- `-object filter-dump,id=f1,netdev=net0,file=net_dump.pcap` on the QEMU command line
  captures ground-truth traffic, read with `tcpdump -r net_dump.pcap -n -e -v` (or `-xx`
  for a raw hex dump byte-for-byte comparison against what the kernel built).
- The QEMU monitor (`info pci` for BAR/IRQ assignment; `i /1xb <port>` / `i /2xw <port>` for
  raw I/O-port reads) confirms hardware/register state independent of what the driver logs.

If a kernel log line and one of these disagree, the external tool is right.
