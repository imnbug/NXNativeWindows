// The null path's three init bumps: the first span, the 2 MiB bump from the phone log, and the third bump.
#include "../ue_span.h"
#include <stdio.h>
static int fails;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "fail %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
static int hi(uint64_t cursor, uint64_t length, uint64_t end) { return cursor + length > end; }
// One init, in order. Returns 0 when every bump fits and one more byte does not.
static int sequence(uint64_t base, uint64_t reserved, uint64_t first, uint64_t second, uint64_t third) {
    uint64_t end = base + reserved, cursor = base;
    if (hi(cursor, first, end)) return 1;
    cursor += first;
    if (hi(cursor, second, end)) return 2;
    cursor += second;
    if (hi(cursor, third, end)) return 3;
    cursor += third;
    if (cursor != end || !hi(cursor, 1, end)) return 4; // the two later bumps use the tail exactly
    return 0;
}
int main(void) {
    uint64_t base = 0x7013e00000ull, reserved = 0x80000000ull, x21 = 0x97fffffffull;
    uint64_t first, tail, third, end = base + reserved;
    // The qword at main+0x98fc048 is 0, so main+0x1452c fills A and B. The file's 0x100000000 at +0x80 is not used.
    CHECK(dm_ue_span_x21(reserved, base, 0, 0, 0x100000000ull, &x21));
    first = x21 + 1;
    tail = reserved - first;
    third = dm_ue_span_third(tail - 1, reserved);
    CHECK(first == reserved - (DM_UE_SPAN_SECOND + DM_UE_SPAN_MIN));
    CHECK(tail == DM_UE_SPAN_SECOND + DM_UE_SPAN_MIN);
    CHECK(third == DM_UE_SPAN_MIN);
    CHECK(reserved <= (first << 1)); // the first request stays its exact length
    CHECK(sequence(base, reserved, first, DM_UE_SPAN_SECOND, third) == 0);
    CHECK(hi(base + first, DM_UE_SPAN_SECOND, end) == 0);
    CHECK(hi(base + first + DM_UE_SPAN_SECOND, third, end) == 0);
    // The phone log: a 0x10000 tail lets the first request through and the 2 MiB bump fails.
    CHECK(hi(base + (reserved - DM_UE_SPAN_MIN), DM_UE_SPAN_SECOND, end));
    CHECK(sequence(base, reserved, reserved - DM_UE_SPAN_MIN, DM_UE_SPAN_SECOND, DM_UE_SPAN_MIN) == 2);
    // The live pair does not track the region. B - A - 1 = 0xffffffff, and the third bump is 0x40000.
    x21 = 0x97fffffffull;
    CHECK(dm_ue_span_x21(reserved, base, 1, 0x1000, 0x1000ull + 0x100000000ull, &x21));
    first = x21 + 1;
    third = dm_ue_span_third(0xffffffffull, reserved);
    CHECK(third == 0x40000ull);
    CHECK(first == reserved - (DM_UE_SPAN_SECOND + third));
    CHECK(sequence(base, reserved, first, DM_UE_SPAN_SECOND, third) == 0);
    // A equals the region base but B is not its end: the diff would depend on both, so the length stays compiled.
    x21 = 0x97fffffffull;
    CHECK(!dm_ue_span_x21(reserved, base, 1, base, 0x100000000ull, &x21) && x21 == 0x97fffffffull);
    x21 = 0x97fffffffull;
    CHECK(!dm_ue_span_x21(DM_UE_SPAN_DESIGNED, base, 0, 0, 0, &x21) && x21 == 0x97fffffffull);
    CHECK(!dm_ue_span_x21(0xf80000000ull, base, 0, 0, 0, &x21));
    CHECK(!dm_ue_span_x21(DM_UE_SPAN_SECOND + DM_UE_SPAN_MIN, base, 0, 0, 0, &x21));
    CHECK(!dm_ue_span_x21(0x80000001ull, base, 0, 0, 0, &x21));
    // The designed 62 GiB region and 38 GiB first span leave a diff whose third bump fits after the 2 MiB one.
    {
        uint64_t designed_tail = 0xf80000000ull - DM_UE_SPAN_DESIGNED;
        uint64_t designed_third = dm_ue_span_third(designed_tail - 1, 0xf80000000ull);
        CHECK(designed_third == DM_UE_SPAN_SECOND);
        CHECK(designed_tail >= DM_UE_SPAN_SECOND + designed_third);
    }
    if (fails) return 1;
    puts("ue span: first span, 2 MiB bump, third bump");
    return 0;
}
