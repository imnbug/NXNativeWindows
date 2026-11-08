// The sdk+0x1464f0 region-tree checks. A result other than map is 0xe03 before the svc.
#include "../ue_pages.h"
#include <stdio.h>
static int fails;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "fail %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
static dm_ue_page_node N(int left, int right, int parent, uint64_t start, uint64_t length) {
    dm_ue_page_node n = { left, right, parent, start, length };
    return n;
}
int main(void) {
    dm_ue_page_node one = N(-1, -1, -1, 0x1000, 0x100);
    CHECK(dm_ue_pages_classify(NULL, 0, -1, 0x1000, 0x100) == DM_UE_PAGES_EMPTY);
    CHECK(dm_ue_pages_classify(&one, 1, 0, 0x1000, 0x100) == DM_UE_PAGES_MAP);
    CHECK(dm_ue_pages_classify(&one, 1, 0, 0x1000, 0x101) == DM_UE_PAGES_EXACT_SHORT);
    // Exact start does not test the carry of address+size. length covers size, so this maps.
    one.start = ~0ull - 8;
    CHECK(dm_ue_pages_classify(&one, 1, 0, ~0ull - 8, 16) == DM_UE_PAGES_MAP);
    one.start = 0x1000;
    // Address above the only key never branches left, so there is no candidate and no predecessor.
    CHECK(dm_ue_pages_classify(&one, 1, 0, 0x1000 + 0x320000000ull, 0x2000) == DM_UE_PAGES_NO_CANDIDATE);
    CHECK(dm_ue_pages_classify(&one, 1, 0, 0x1000 + 0x10, 0x10) == DM_UE_PAGES_NO_CANDIDATE);
    // Address below every key, parent null: no predecessor.
    CHECK(dm_ue_pages_classify(&one, 1, 0, 0, 0x10) == DM_UE_PAGES_NO_PRED);
    // A higher key makes the search branch left. Its parent is then the predecessor.
    dm_ue_page_node cov[2] = {
        N(-1, 1, -1, 0x7014000000ull, 0x80000000ull),
        N(-1, -1, 0, 0x7094000000ull, 0),
    };
    CHECK(dm_ue_pages_classify(cov, 2, 0, 0x7014000000ull + 0x1000, 0x2000) == DM_UE_PAGES_MAP);
    CHECK(dm_ue_pages_classify(cov, 2, 0, 0x7014000000ull + 0x80000000ull - 0x1000, 0x2000) == DM_UE_PAGES_END_PAST);
    // Past every key: the end node is still below the address, and nothing is left to branch to.
    CHECK(dm_ue_pages_classify(cov, 2, 0, 0x7014000000ull + 0x320000000ull, 0x2000) == DM_UE_PAGES_NO_CANDIDATE);
    // The predecessor's start is above the address (the link is not a search tree). +0x1465a0.
    dm_ue_page_node above[2] = {
        N(-1, -1, 1, 80, 1),
        N(0, 0, -1, 90, 1),
    };
    CHECK(dm_ue_pages_classify(above, 2, 0, 50, 1) == DM_UE_PAGES_START_ABOVE);
    dm_ue_page_node wide[2] = {
        N(-1, 1, -1, 10, 100),
        N(-1, -1, 0, ~0ull - 4, 1),
    };
    CHECK(dm_ue_pages_classify(wide, 2, 0, ~0ull - 8, 16) == DM_UE_PAGES_CARRY);
    dm_ue_page_node loop = N(0, -1, -1, 1, 1);
    CHECK(dm_ue_pages_classify(&loop, 1, 0, 0, 1) == DM_UE_PAGES_BROKEN);
    if (fails) return 1;
    puts("ue pages: e03 is the region tree, before the svc");
    return 0;
}
