// Minecraft Dungeons II 1.1.3.0 main c6b32f4d3d4243ff190b082b4f6aea18.
// The null-argument path builds x21 = 0x97fffffff and then executes add x0, x21, #1, so the
// bump request is 0x980000000 (38 GiB). The arena end is the reserved region: init at main+0x28a74
// stores base at +0x28, base+length at +0x30, and length at +0x38. The bump at main+0x28b20 returns
// the old cursor and adds the request to it. The check is b.hi (cursor + request > end), so a
// request equal to the arena length fits and leaves the cursor on the end. One later bump then fails.
// The same init stores 0x10000 at +0x48, and that is the minimum size the slow path asks for, so the
// substituted request keeps that much of the arena unused.
#pragma once
#include <stdint.h>

enum { DM_UE_SPAN_DESIGNED = 0x980000000ull, DM_UE_SPAN_QUANTUM = 0x10000ull };

// 1 = *x21 becomes (reserved − quantum) − 1, and add x0, x21, #1 yields the request.
// 0 = leave the compiled 38 GiB length. A reserved length of the designed size or more already fits.
static inline int dm_ue_span_x21(uint64_t reserved, uint64_t *x21) {
    if (!x21 || reserved >= DM_UE_SPAN_DESIGNED) return 0;
    if (reserved <= DM_UE_SPAN_QUANTUM || (reserved & 0xfffull)) return 0;
    *x21 = reserved - DM_UE_SPAN_QUANTUM - 1;
    return 1;
}
