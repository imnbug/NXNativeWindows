// VammManager::AllocateMemoryPages at sdk+0x1464f0 (Minecraft Dungeons II's sdk). The tree is
// [manager+0x40] then +0x10. A node holds left +0, right +8, parent +0x10, start +0x20, length +0x28.
// 0xe03 is returned before the svc at +0x17572c. +0x146578 covers an empty tree, no candidate, and an
// exact start whose length is shorter than the request. +0x1465a0 is preset for the carry of
// address+size, a predecessor start above the address, a request end past that predecessor, and no
// predecessor. Kernel 0xD001 and 0x10801 become 0x1003 and 0xCE01 becomes 0x1203 after the svc;
// 0xD401 is not turned into 0xe03. parent/left/right are indices, -1 for a null link.
#pragma once
#include <stdint.h>

enum {
    DM_UE_PAGES_MAP = 0,
    DM_UE_PAGES_EMPTY = 1,
    DM_UE_PAGES_NO_CANDIDATE = 2,
    DM_UE_PAGES_EXACT_SHORT = 3,
    DM_UE_PAGES_CARRY = 4,
    DM_UE_PAGES_START_ABOVE = 5,
    DM_UE_PAGES_END_PAST = 6,
    DM_UE_PAGES_NO_PRED = 7,
    DM_UE_PAGES_BROKEN = 8,
};

typedef struct { int left, right, parent; uint64_t start, length; } dm_ue_page_node;

static inline const char *dm_ue_pages_name(int c) {
    switch (c) {
    case DM_UE_PAGES_MAP: return "map";
    case DM_UE_PAGES_EMPTY: return "empty";
    case DM_UE_PAGES_NO_CANDIDATE: return "no-candidate";
    case DM_UE_PAGES_EXACT_SHORT: return "exact-short";
    case DM_UE_PAGES_CARRY: return "carry";
    case DM_UE_PAGES_START_ABOVE: return "start-above";
    case DM_UE_PAGES_END_PAST: return "end-past";
    case DM_UE_PAGES_NO_PRED: return "no-predecessor";
    case DM_UE_PAGES_BROKEN: return "broken";
    default: return "unknown";
    }
}

static inline int dm_ue_pages_classify(const dm_ue_page_node *n, int count, int root, uint64_t addr, uint64_t size) {
    int x8, x9, guard = 0;
    uint64_t start, end;
    if (root < 0) return DM_UE_PAGES_EMPTY;
    if (root >= count) return DM_UE_PAGES_BROKEN;
    x9 = -1;
    x8 = root;
    goto cmp;
left:
    x9 = x8;
    x8 = n[x8].left;
    if (x8 < 0) goto use_candidate;
    if (x8 >= count || ++guard > count + 1) return DM_UE_PAGES_BROKEN;
cmp:
    start = n[x8].start;
    if (start > addr) goto left;
    if (start == addr) goto check_exact;
    x8 = n[x8].right;
    if (x8 < 0) goto use_candidate;
    if (x8 >= count || ++guard > count + 1) return DM_UE_PAGES_BROKEN;
    goto cmp;
use_candidate:
    if (x9 < 0) return DM_UE_PAGES_NO_CANDIDATE;
    if (x9 >= count) return DM_UE_PAGES_BROKEN;
    start = n[x9].start;
    x8 = x9;
check_exact:
    if (start != addr) goto not_exact;
    if (n[x8].length >= size) return DM_UE_PAGES_MAP;
    return DM_UE_PAGES_EXACT_SHORT;
not_exact:
    x9 = n[x8].left;
    if (x9 >= count) return DM_UE_PAGES_BROKEN;
    if (x9 >= 0) {
        int spin = 0;
        while (n[x9].right >= 0) {
            x9 = n[x9].right;
            if (x9 >= count || ++spin > count) return DM_UE_PAGES_BROKEN;
        }
        goto range;
    }
    x9 = n[x8].parent;
    if (x9 >= 0 && x9 < count && n[x9].right == x8) goto range;
    if (x9 >= count) return DM_UE_PAGES_BROKEN;
    for (int spin = 0;; spin++) {
        x9 = n[x8].parent;
        if (x9 < 0) return DM_UE_PAGES_NO_PRED;
        if (x9 >= count || ++spin > count) return DM_UE_PAGES_BROKEN;
        if (n[x9].left != x8) goto range;
        x8 = x9;
    }
range:
    if (__builtin_add_overflow(addr, size, &end)) return DM_UE_PAGES_CARRY;
    if (n[x9].start > addr) return DM_UE_PAGES_START_ABOVE;
    if (end > n[x9].start + n[x9].length) return DM_UE_PAGES_END_PAST;
    return DM_UE_PAGES_MAP;
}
