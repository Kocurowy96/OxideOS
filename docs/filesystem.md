# Filesystem

OxideOS uses a real **ext2** filesystem — not a custom or simplified format. Disk images
are built with genuine `mke2fs`/`debugfs` (see [`tooling.md`](tooling.md)), so `disk.img`
can be inspected, fixed, or modified with any standard Linux ext2 tooling
(`debugfs -R "cat ..."`, `e2fsck -f`, mounting it via a loop device), independent of
OxideOS itself — this is deliberate and has been the main way of cross-checking the
in-kernel driver's behavior during development.

## Layers

```
apps/*.ELF  --sys_read_file/sys_write_file-->  kernel/cpu/syscall.cpp
                                                       |
                                                       v
                                              kernel/fs/vfs.cpp  (VFS::)
                                                       |
                                                       v
                                              kernel/fs/ext2.cpp  (Ext2::)
                                                       |
                                                       v
                                          kernel/drivers/ata.cpp  (ATA::ReadSector/WriteSector)
```

`VFS` (`kernel/fs/vfs.cpp`) is a thin pass-through to `Ext2::` today — `Init`, `ReadFile`,
`WriteFile`, `FreeFile`, `ListDirectory` all forward directly. It exists as the seam where a
second filesystem or device type could be added later without touching every call site in
the kernel/apps.

An older **FAT32** driver (`kernel/fs/fat32.*`) still exists in the tree but is unwired from
the build (commented out of `CMakeLists.txt`) — kept in case a future real-hardware
USB-stick scenario wants FAT32 rather than ext2, not because it's still in use.

## `Ext2::` — what's implemented

- **Reads**: `ResolvePath` walks directory entries from the root inode; `ReadFile` handles
  files using direct blocks (`inode.block[0..11]`) and singly-indirect blocks
  (`inode.block[12]`). Doubly/triply-indirect blocks (`block[13]`/`block[14]`) are **not**
  implemented — very large files will fail to read.
- **Writes**: `WriteFile` handles both brand-new files (allocates an inode + direct blocks,
  adds a directory entry) and overwriting an existing file (grows/shrinks the direct block
  list, updates `size`, directory entry untouched). Like reads, writes are only implemented
  for files fitting in direct blocks — a write requiring the indirect block to be
  *allocated* (not just read) is not yet handled.
- **Allocation**: real bitmap-based `AllocateBlock`/`FreeBlock` and
  `AllocateInode`/`FreeInode`, scanning the block/inode bitmaps described by the block
  group descriptor table — not a simplified always-append scheme.
- **I/O**: `ReadDiskSector`/`WriteDiskSector` go through `ATA::ReadSector`/`WriteSector`
  (primary master, i.e. `-hda` in QEMU). During early development (ext2 driver bring-up
  before it replaced FAT32) these temporarily pointed at the ATA slave (`-hdb`) so the
  driver could be exercised against a throwaway image without risking the filesystem that
  was still in production use at the time — that's long since switched back; today it's
  the real, only disk.

## Known limitations

- No journaling (it's ext2, not ext3/4) — an unclean shutdown on real hardware could leave
  the filesystem in a state `e2fsck` needs to repair. Acceptable for a hobby OS today; would
  need revisiting if OxideOS is ever meant to survive real power loss gracefully.
- Files needing doubly/triply-indirect blocks are unsupported for both read and write (see
  above) — fine for the small config/text files the system currently deals with
  (`/NOTES.DAT`, `/notatka.txt`, `/DOCS/CONFIG.DAT`, app `.ELF` binaries), would need work
  before hosting anything large (e.g. downloaded packages — see the package manager plans).

## Verifying filesystem changes

The standard way to independently check anything the ext2 driver does (used throughout
development, not just historically): after a headless test run that wrote to disk,
`debugfs -R "cat /path/to/file" disk.img` to read back exactly what's on disk, and
`e2fsck -f disk.img` to confirm the filesystem structure itself is still consistent. Both
are ordinary Linux tools with no dependency on OxideOS's own code — if they disagree with
what the kernel log claims happened, trust them over the log.
