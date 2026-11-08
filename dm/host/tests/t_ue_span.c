// The null path's init bumps, plus the empty-freelist refill that stores 16 bytes at the returned address.
#include "../ue_span.h"
#include <stdio.h>
static int fails;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "fail %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
static int hi(uint64_t cursor, uint64_t length, uint64_t end) { return cursor + length > end; }
// main+0x28c10: offset += 0x10, continue while offset + 0x20 <= size, and each step stores 16 bytes.
// Returns 1 when a store starts at or past end, or extends past it.
static int refill_store_faults(uint64_t ptr, uint64_t size, uint64_t end) {
    uint64_t off = 0;
    for (;;) {
        if (ptr + off >= end || ptr + off + 0x10 > end) return 1;
        if (off + 0x20 > size) break;
        off += 0x10;
    }
    return off != size - 0x10;
}
// first, second, third, then the refill. 0 = every store stays inside and the cursor finishes on end.
static int sequence(uint64_t base, uint64_t reserved, uint64_t first, uint64_t second, uint64_t third) {
    uint64_t end = base + reserved, cursor = base, refill;
    if (hi(cursor, first, end)) return 1;
    cursor += first;
    if (hi(cursor, second, end)) return 2;
    cursor += second;
    if (hi(cursor, third, end)) return 3;
    cursor += third;
    refill = cursor;
    if (hi(cursor, DM_UE_SPAN_MIN, end)) return 4;
    if (refill_store_faults(refill, DM_UE_SPAN_MIN, end)) return 5;
    cursor += DM_UE_SPAN_MIN;
    if (cursor != end || !hi(cursor, 1, end)) return 6;
    return 0;
}
int main(void) {
    uint64_t base = 0x7013e00000ull, reserved = 0x80000000ull, x21 = 0x97fffffffull;
    uint64_t first, tail, third, end = base + reserved;
    // Sentinel 0: main+0x1452c fills A and B. The file's 0x100000000 at +0x80 is not used.
    CHECK(dm_ue_span_x21(reserved, base, 0, 0, 0x100000000ull, &x21));
    first = x21 + 1;
    tail = reserved - first;
    third = dm_ue_span_third(tail - 1, reserved);
    CHECK(first == reserved - (DM_UE_SPAN_SECOND + DM_UE_SPAN_MIN + DM_UE_SPAN_MIN));
    CHECK(tail == 0x220000ull);
    CHECK(third == DM_UE_SPAN_MIN);
    CHECK(reserved <= (first << 1));
    CHECK(sequence(base, reserved, first, DM_UE_SPAN_SECOND, third) == 0);
    // V2's tail of 0x210000: the three bumps end on the arena end, and the refill store is at that address.
    {
        uint64_t v2 = reserved - 0x210000ull;
        CHECK(!hi(base, v2, end) && !hi(base + v2, DM_UE_SPAN_SECOND, end) && !hi(base + v2 + DM_UE_SPAN_SECOND, DM_UE_SPAN_MIN, end));
        CHECK(base + v2 + DM_UE_SPAN_SECOND + DM_UE_SPAN_MIN == end);
        CHECK(refill_store_faults(end, DM_UE_SPAN_MIN, end));
    }
    // A 0x10000 tail lets the first request through and the 2 MiB bump fails.
    CHECK(hi(base + (reserved - DM_UE_SPAN_MIN), DM_UE_SPAN_SECOND, end));
    // The live pair does not track the region. B - A - 1 = 0xffffffff. align_up(0x1000 << 5, 0x1000) = 0x20000.
    x21 = 0x97fffffffull;
    CHECK(dm_ue_span_x21(reserved, base, 1, 0x1000, 0x1000ull + 0x100000000ull, &x21));
    first = x21 + 1;
    third = dm_ue_span_third(0xffffffffull, reserved);
    CHECK(third == 0x20000ull);
    CHECK(first == reserved - (DM_UE_SPAN_SECOND + third + DM_UE_SPAN_MIN));
    CHECK(sequence(base, reserved, first, DM_UE_SPAN_SECOND, third) == 0);
    // A equals the region base but B is larger than the region end: the guest would subtract the span length.
    x21 = 0x97fffffffull;
    CHECK(!dm_ue_span_x21(reserved, base, 1, base, base + 0x100000000ull, &x21) && x21 == 0x97fffffffull);
    x21 = 0x97fffffffull;
    CHECK(!dm_ue_span_x21(reserved, base, 1, base, 0x100000000ull, &x21) && x21 == 0x97fffffffull);
    CHECK(!dm_ue_span_x21(reserved, UINT64_MAX - 0x1000, 0, 0, 0, &x21));
    x21 = 0x97fffffffull;
    CHECK(!dm_ue_span_x21(DM_UE_SPAN_DESIGNED, base, 0, 0, 0, &x21) && x21 == 0x97fffffffull);
    CHECK(!dm_ue_span_x21(0xf80000000ull, base, 0, 0, 0, &x21));
    CHECK(!dm_ue_span_x21(DM_UE_SPAN_SECOND + DM_UE_SPAN_MIN + DM_UE_SPAN_MIN, base, 0, 0, 0, &x21));
    CHECK(!dm_ue_span_x21(0x80000001ull, base, 0, 0, 0, &x21));
    {
        uint64_t designed_tail = 0xf80000000ull - DM_UE_SPAN_DESIGNED;
        uint64_t designed_third = dm_ue_span_third(designed_tail - 1, 0xf80000000ull);
        CHECK(designed_third == 0x100000ull);
        CHECK(designed_tail >= DM_UE_SPAN_SECOND + designed_third + DM_UE_SPAN_MIN);
    }
    if (fails) return 1;
    puts("ue span: three bumps and the freelist refill inside the arena");
    return 0;
}
