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
```

`kernel/net/config.h` holds the static configuration: `NET_OUR_IP` (`10.0.2.15`) and
`NET_GATEWAY_IP` (`10.0.2.2`) — matched to QEMU's default user-mode networking (SLIRP)
subnet. There is no DHCP yet; the IP is hardcoded (see the roadmap for DHCP as the next
networking phase).

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

## QEMU networking model — what "works" actually means

`-netdev user` (SLIRP) gives the guest NAT-style access to a **private** `10.0.2.0/24`
subnet (guest = `10.0.2.15`, gateway = `10.0.2.2`) that is **not reachable from the real
host** — `ping 10.0.2.15` from the host returns "Destination Net Unreachable"; only
guest-initiated traffic, or `hostfwd=tcp/udp::PORT-:GUESTPORT` (for deliberately testing
"something outside contacts the guest"), actually crosses that boundary. This is why the
network milestone is framed as *"OxideOS pings out and gets a real reply"*, not *"the host
can ping OxideOS"* — the latter was the original plan and turned out to be impossible with
this QEMU networking mode; corrected once discovered empirically.

## Independent verification

Every phase of this stack was checked against tools that don't trust the kernel's own
account of what happened:

- `-object filter-dump,id=f1,netdev=net0,file=net_dump.pcap` on the QEMU command line
  captures ground-truth traffic, read with `tcpdump -r net_dump.pcap -n -e -v` (or `-xx`
  for a raw hex dump byte-for-byte comparison against what the kernel built).
- The QEMU monitor (`info pci` for BAR/IRQ assignment; `i /1xb <port>` / `i /2xw <port>` for
  raw I/O-port reads) confirms hardware/register state independent of what the driver logs.

If a kernel log line and one of these disagree, the external tool is right.
