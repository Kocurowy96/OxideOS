#pragma once

#include <stdint.h>

// Critical UI assets (cursor, generic window icon) baked directly into the
// kernel binary so the GUI still has a cursor and an icon even if the ext2
// filesystem fails to mount or the files are missing from disk.img. These
// used to be loaded at runtime via VFS::ReadFile("/icon.bmp") and
// VFS::ReadFile("/cursor_normal.bmp") (see kernel/gui/compositor.cpp).
//
// embedded_assets.cpp is generated, not hand-written. To regenerate after
// changing the source art, reproduce the exact bytes scripts/make_disk.sh
// writes to disk.img and dump them as C arrays:
//
//   python3 -c "<the icon.bmp fallback generator from scripts/make_disk.sh>"
//   convert assets/cursor_normal.png -define bmp:format=bmp3 \
//       -define bmp3:alpha=true BMP3:cursor_normal.bmp
//   (then turn icon.bmp / cursor_normal.bmp into uint8_t[] arrays, 16 bytes
//   per line, named EmbeddedAssets::icon_bmp / EmbeddedAssets::cursor_normal_bmp)
namespace EmbeddedAssets {
    extern uint8_t icon_bmp[];
    extern const uint32_t icon_bmp_len;

    extern uint8_t cursor_normal_bmp[];
    extern const uint32_t cursor_normal_bmp_len;
}
