// The null-path length is the reserved region minus one minimum bump, and x21 stays length − 1.
#include "../ue_span.h"
#include <stdio.h>
static int fails;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "fail %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
static int hi(uint64_t cursor, uint64_t length, uint64_t end) { return cursor + length > end; }
int main(void) {
    uint64_t base = 0x7013e00000ull, reserved = 0x80000000ull, end = base + reserved, x21 = 0x97fffffffull;
    CHECK(dm_ue_span_x21(reserved, &x21));
    CHECK(x21 + 1 == reserved - DM_UE_SPAN_QUANTUM);
    CHECK(!hi(base, x21 + 1, end));
    CHECK(!hi(base + (x21 + 1), DM_UE_SPAN_QUANTUM, end));
    CHECK(hi(base + (x21 + 1) + DM_UE_SPAN_QUANTUM, 1, end));
    x21 = 0x97fffffffull;
    CHECK(!dm_ue_span_x21(DM_UE_SPAN_DESIGNED, &x21) && x21 == 0x97fffffffull);
    CHECK(!dm_ue_span_x21(0xf80000000ull, &x21));
    CHECK(!dm_ue_span_x21(0x10000, &x21));
    CHECK(!dm_ue_span_x21(0x80000001ull, &x21));
    uint64_t full = base;
    CHECK(!hi(full, reserved, end));
    if (fails) return 1;
    puts("ue span: reserved length minus one minimum bump, b.hi strict");
    return 0;
}
