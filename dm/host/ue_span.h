// Minecraft Dungeons II 1.1.3.0 main c6b32f4d3d4243ff190b082b4f6aea18.
// main+0xb0560's null-argument path takes three bumps from the arena whose length is the reserved
// region at main+0x997e1e0 and whose cursor starts at the region base at main+0x997e1d8. main+0x28b20
// stores the new cursor before the caller's b.hi, and b.hi is strict (cursor + request > end).
//   1. main+0xb0648 is add x0, x21, #1. The compiled x21 is 0x97fffffff, so the request is 0x980000000.
//   2. main+0xb0834. The 76-bin loop does not read x21. On this build the bump is 0x200000: the crash at
//      main+0x141619c had x21 = x24 = 0x200000 and x23 = 21. A 2 GiB arena is larger than twice that
//      request, so 0x14ab0 rounds it to that power of two.
//   3. main+0xb0a34. stored = (1 << ((-clz(diff)) & 63)) >> 20, from the constants at main+0xb0670 for
//      page 0x1000. The byte length is align_up(0x1000 + (stored << 5), 0x1000). 0x14964 and 0x14ab0
//      then round up to the next power of two when the arena is larger than twice that length, and the
//      bump is at least the 0x10000 stored at the arena object +0x48.
//      diff = B - x11 - 1. A, B are ldp [main+0x14514's return, +0x30]. That function returns the address
//      of main+0x98fc048. When the qword there is 0, main+0x1452c runs first and stores the region base
//      at +0x30 and base + reserved at +0x38. The first bump returns that base, the compare at main+0xb06ac
//      is equal, and x11 = base + first length, so diff = reserved - first length - 1. When the qword is
//      already nonzero, +0x30 and +0x38 are the live values, relocation included. Unless they are that
//      same base and end, x11 = A and diff = B - A - 1, which does not contain the span length.
//      The file image of those qwords (0, 0, 0x100000000) is not the runtime value.
// main+0x14d10 runs after the second and third returns. It passes the block just returned to main+0x28f50;
// the caller then memsets that same pointer. It does not call main+0x14964. The constructor returns at
// main+0xb0a98. A reserved length of the designed 0x980000000 or more already holds the compiled first
// request, so x21 is left unchanged.
#pragma once
#include <stdint.h>

enum { DM_UE_SPAN_DESIGNED = 0x980000000ull,
       DM_UE_SPAN_SECOND = 0x200000ull,
       DM_UE_SPAN_MIN = 0x10000ull,
       DM_UE_SPAN_PAGE = 0x1000ull };

// The bump 0x14ab0 takes for a byte length: the exact length, or the next power of two when the arena
// is larger than twice the length, and at least the minimum at the arena object +0x48.
static inline uint64_t dm_ue_span_bump(uint64_t bytes, uint64_t reserved) {
    uint64_t aligned = (bytes + (DM_UE_SPAN_PAGE - 1)) & ~(uint64_t)(DM_UE_SPAN_PAGE - 1);
    uint64_t block = aligned;
    if (aligned <= (UINT64_MAX >> 1) && reserved > (aligned << 1)) {
        uint64_t n = aligned ? aligned - 1 : 0;
        int bits = n ? 64 - __builtin_clzll(n) : 0;
        block = bits >= 64 ? UINT64_MAX : (bits ? (1ull << bits) : 0);
    }
    return block > DM_UE_SPAN_MIN ? block : DM_UE_SPAN_MIN;
}

// stored at [x19+0x20], then the third bump, for diff = B - x11 - 1.
static inline uint64_t dm_ue_span_stored(uint64_t diff) {
    int c = diff ? __builtin_clzll(diff) : 64;
    return (1ull << ((-c) & 63)) >> 20;
}
static inline uint64_t dm_ue_span_third(uint64_t diff, uint64_t reserved) {
    return dm_ue_span_bump(DM_UE_SPAN_PAGE + (dm_ue_span_stored(diff) << 5), reserved);
}

// 1 = *x21 becomes (reserved - tail) - 1, and add x0, x21, #1 yields the first request.
// tail is the second bump plus the third bump. 0 = leave the compiled 38 GiB length.
// sentinel, field_a and field_b are the qwords at main+0x98fc048, +0x78 and +0x80 as they are when
// the gate runs, which is before main+0x14514. region_base is the qword at main+0x997e1d8.
static inline int dm_ue_span_x21(uint64_t reserved, uint64_t region_base, uint64_t sentinel,
                                 uint64_t field_a, uint64_t field_b, uint64_t *x21) {
    uint64_t tail, third;
    int same;
    if (!x21 || reserved >= DM_UE_SPAN_DESIGNED || (reserved & (DM_UE_SPAN_PAGE - 1))) return 0;
    if (!sentinel)
        same = 1; // main+0x1452c overwrites +0x30/+0x38 with the region base and its end
    else if (reserved <= UINT64_MAX - region_base && field_a == region_base && field_b == region_base + reserved)
        same = 1;
    else if (field_b > field_a)
        same = 0;
    else
        return 0;
    if (same) {
        tail = DM_UE_SPAN_SECOND + DM_UE_SPAN_MIN;
        // diff = tail - 1. The third bump is the minimum, so the tail is exactly the two later bumps.
        if (dm_ue_span_third(tail - 1, reserved) != DM_UE_SPAN_MIN) return 0;
    } else {
        third = dm_ue_span_third(field_b - field_a - 1, reserved);
        if (third < DM_UE_SPAN_MIN || DM_UE_SPAN_SECOND > UINT64_MAX - third) return 0;
        tail = DM_UE_SPAN_SECOND + third;
    }
    // reserved >= 2 * tail keeps the first request off the power-of-two path (reserved > 2 * length is false).
    if (tail >= reserved || reserved / 2 < tail) return 0;
    *x21 = reserved - tail - 1;
    return 1;
}
