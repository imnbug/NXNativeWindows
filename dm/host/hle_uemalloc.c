// Unreal Engine's allocator on a host malloc zone. Octopath Traveler 0's engine allocator is an FMallocBinned3 constructed in place in nninitStartup; its constructor reserves 37 GB of
// address space (74 pools of 512 MB), which an iPhone cannot give (about 5 GB without the extended virtual addressing entitlement), and its pools are too uneven to shrink (CUTS in
// the game's patches.py: the constructor call is cut). The host puts this vtable in the object instead: every FMemory allocation reaches it through the object (FPlatformMemory::BaseAllocator returns it,
// and the thread-local storage allocator calls its Malloc / Free slots), and the guest's own trivial functions stay in the slots that do nothing.
// The FMalloc slots (the engine's FMalloc interface, measured from the vtable and the thread-local callbacks): 0-1 destructors, 2-5 FExec, 6 Malloc, 7 TryMalloc, 8 Realloc, 9 TryRealloc, 10 Free, 11 QuantizeSize, 12 GetAllocationSize, 13 Trim, 14 SetupTLSCachesOnCurrentThread,
// 15 ClearAndDisableTLSCachesOnCurrentThread, 16 InitializeStatsMetadata, 17 UpdateStats, 18 GetAllocatorStats, 19 DumpAllocatorStats, 20 IsInternallyThreadSafe, 21 ValidateHeap,
// 22 GetDescriptiveName, 23-25 OnMallocInitialized / OnPreFork / OnPostFork (the thread-local callbacks main+0x12e80 / main+0x12ec0 call +0x30 and +0x50).
// Unreal Engine 4.27 (DRAGON QUEST VII Reimagined) has one FExec slot and no fork callbacks: 0-1 destructors, 2 Exec, 3 Malloc, 4 TryMalloc, 5 Realloc, 6 TryRealloc, 7 Free, 8 QuantizeSize,
// 9 GetAllocationSize, 10 Trim, 11 SetupTLSCachesOnCurrentThread, 12 ClearAndDisableTLSCachesOnCurrentThread, 13 InitializeStatsMetadata, 14 UpdateStats, 15 GetAllocatorStats,
// 16 DumpAllocatorStats, 17 IsInternallyThreadSafe, 18 ValidateHeap, 19 GetDescriptiveName, 20 OnMallocInitialized (main+0x5e4c0e8: TryMalloc / TryRealloc call +0x18 / +0x28, GCreateMalloc
// calls +0x88 and +0xa0; Free is main+0x10, which reads the pool base as GetAllocationSize does). Its allocator object is made at run time (GCreateMalloc mallocs it), so the host puts the
// vtable in at an observation gate right after the cut constructor call (OBS in the game's patches.py, OBS_UE_MALLOC: x19 = the object)
// Unreal Engine 5.6 (FINAL FANTASY RESONANCE) adds MallocZeroed / TryMallocZeroed after Free and two TLS cache calls after the setup, and two cached-memory queries at the end: 0-1 destructors,
// 2-5 FExec, 6 Malloc, 7 TryMalloc, 8 Realloc, 9 TryRealloc, 10 Free, 11 MallocZeroed, 12 TryMallocZeroed, 13 QuantizeSize, 14 GetAllocationSize, 15 Trim, 16 SetupTLSCachesOnCurrentThread,
// 17 MarkTLSCachesAsUsedOnCurrentThread, 18 MarkTLSCachesAsUnusedOnCurrentThread, 19 ClearAndDisableTLSCachesOnCurrentThread, 20 InitializeStatsMetadata, 21 UpdateStats, 22 GetAllocatorStats,
// 23 DumpAllocatorStats, 24 IsInternallyThreadSafe, 25 ValidateHeap, 26 GetDescriptiveName, 27-29 OnMallocInitialized / OnPreFork / OnPostFork, 30-31 the free cached memory sizes (main+0x67df800;
// the base FMalloc vtable main+0x67dff48 has 0 for the three pure slots 6 / 8 / 10, its 7 / 9 call +0x30 / +0x40, and its 11 and 12 are one function: +0x30, then memset). The zeroed slots
// keep the guest's function, which calls Malloc through the vtable
// Fixed addresses: each row runs only on the main build it was found in (AGENTS.md)
#include "hle.h"
#include "ue_span.h"
#include "ue_pages.h"
#include <malloc/malloc.h>
#include <mach/mach.h>
#include <stdatomic.h>

// Slot layouts: which slots the host fills and which do nothing (up to 6; 0 ends the list); the others keep the guest's own functions
enum { UE5, UE4_27, UE5_6 };
static const struct { int nslot, malloc_, try_malloc, realloc_, try_realloc, free_, quantize, get_size, nothing[6]; } k_lay[] = {
    [UE5] = { 26, 6, 7, 8, 9, 10, 11, 12, { 13, 14, 15, 19 } },      // Trim, the TLS cache setup and teardown, DumpAllocatorStats
    [UE4_27] = { 21, 3, 4, 5, 6, 7, 8, 9, { 10, 11, 12, 16 } },
    [UE5_6] = { 32, 6, 7, 8, 9, 10, 13, 14, { 15, 16, 17, 18, 19, 23 } },   // Trim, the four TLS cache calls, DumpAllocatorStats
};
static const struct { const char *build, *what; int layout; uint64_t obj, vtable, cut; } k_ue[] = {
    // Octopath Traveler 0 1.0.5: the FMallocBinned3 object (main+0x68022c8, bss), its vtable (main+0x5b06f38, 26 slots), the cut constructor call (main+0x12e40, a nop after the rewrite)
    { "9fe60a212cff9b9a3656ab63d4f882e5", "Octopath Traveler 0 1.0.5", UE5, 0x68022c8, 0x5b06f38, 0x12e40 },
    // Octopath Traveler 0 1.0.1 (v0, the cartridge): the same nninitStartup and constructor (main+0x12e40 → main+0x9f480); the object at main+0x68082c8, its vtable (26 slots) at main+0x5b0cf68
    { "5069fe9069984e0cbf1c7f439501538b", "Octopath Traveler 0 1.0.1", UE5, 0x68082c8, 0x5b0cf68, 0x12e40 },
    // DRAGON QUEST VII Reimagined 1.2.0: no fixed object (0: made by GCreateMalloc, given at the gate), its vtable (main+0x5e4c0e8, 21 slots), the cut constructor call (main+0x32758dc)
    { "f7ec03e4cb9de3bd8319d4e899d2e9cc", "DRAGON QUEST VII Reimagined 1.2.0", UE4_27, 0, 0x5e4c0e8, 0x32758dc },
    // Pikmin 4 1.1.1 (Unreal Engine 4.26.2, the 4.27 slot layout: TryMalloc / TryRealloc call +0x18 / +0x28, slot 17 returns 1): made by GCreateMalloc, its vtable (main+0x8da4758, 21 slots),
    // the cut constructor call (main+0x353f788)
    { "060f827bd2b38703e9ea11e3210d89bc", "Pikmin 4 1.1.1", UE4_27, 0, 0x8da4758, 0x353f788 },
    // OCTOPATH TRAVELER II 1.1.1 (Unreal Engine 4.27): made by GCreateMalloc, its vtable (main+0x64710b0: slots 0-19 as the 4.27 layout, TryMalloc / TryRealloc call +0x18 / +0x28,
    // slots 17 and 18 return 1, 19 the name; there is no OnMallocInitialized, so the 21st word copied is the next vtable's offset to top, which nothing calls), the cut constructor call (main+0xbecc08)
    { "0d9649011312f62e93f6a5673a09d7e9", "OCTOPATH TRAVELER II 1.1.1", UE4_27, 0, 0x64710b0, 0xbecc08 },
    // SHIN MEGAMI TENSEI V: Vengeance 1.0.3: GCreateMalloc makes the object in x19; the constructor reserves 37 GB at main+0x15790b0. Its vtable has the UE4 layout:
    // TryMalloc / TryRealloc call +0x18 / +0x28, slots 17 and 18 return 1, 19 returns "Binned3"; the following word is 0 (no OnMallocInitialized).
    { "0a4f14a5c12f470a4d3168a8f2ae6005", "SHIN MEGAMI TENSEI V: Vengeance 1.0.3", UE4_27, 0, 0x5a7f0a8, 0x1f2f63c },
    // FINAL FANTASY RESONANCE 1.0.1: the FMallocBinned3 object (main+0x77c6210, bss), its vtable (main+0x67df800, 32 slots), the cut constructor call (main+0x137f0)
    { "84e182f72ee1da4f477804bd05b54c15", "FINAL FANTASY RESONANCE 1.0.1", UE5_6, 0x77c6210, 0x67df800, 0x137f0 },
    // Asian release: object, 32-slot vtable and constructor cut verified on this main build (PRs #126 and #304).
    { "b7f965b9d7227db8e29f04e84ea7331b", "FINAL FANTASY RESONANCE 1.0.1 (Asian release)", UE5_6, 0x77c6210, 0x67df800, 0x137f0 },
};
enum { NSLOT = 32 };

static malloc_zone_t *g_zone;
static _Atomic int64_t g_used, g_peak;   // bytes in use (usable sizes) and their high-water mark, for the [mem] line (one per 256 MB of new peak)
static void used(int64_t d) {
    int64_t after = atomic_fetch_add(&g_used, d) + d, peak = atomic_load(&g_peak);
    while (after > peak && !atomic_compare_exchange_weak(&g_peak, &peak, after)) {}
    if (after > peak && after >> 28 != peak >> 28) LOG("[mem] Unreal heap (host malloc zone) up to %.2f GB in use", after / 1073741824.0);
}
static size_t align_of(uint32_t a) { return a > 16 ? a : 16; }   // 0 = DEFAULT_ALIGNMENT, 16 bytes as Binned3 gives
static void *alloc(size_t n, uint32_t a) {
    void *p = align_of(a) == 16 ? malloc_zone_malloc(g_zone, n ? n : 1) : malloc_zone_memalign(g_zone, align_of(a), n ? n : 1);
    if (p) used((int64_t)malloc_size(p));
    return p;
}
static void release(void *p) { if (p) { used(-(int64_t)malloc_size(p)); malloc_zone_free(g_zone, p); } }
static void *realloc_aligned(void *p, size_t n, uint32_t a) {
    if (!p) return alloc(n, a);
    if (!n) { release(p); return NULL; }
    size_t old = malloc_size(p);
    if (align_of(a) == 16) { void *q = malloc_zone_realloc(g_zone, p, n); if (q) used((int64_t)malloc_size(q) - (int64_t)old); return q; }
    if (!((uintptr_t)p & (align_of(a) - 1)) && old >= n) return p;
    void *q = alloc(n, a); if (!q) return NULL;
    memcpy(q, p, old < n ? old : n); release(p); return q;
}

static void *m_Malloc(void *self, size_t n, uint32_t a) {
    (void)self; void *p = alloc(n, a);
    if (!p) die("Unreal heap: out of memory (%zu bytes, alignment %u, %.2f GB in use)", n, a, atomic_load(&g_used) / 1073741824.0);
    return p;
}
static void *m_TryMalloc(void *self, size_t n, uint32_t a) { (void)self; return alloc(n, a); }
static void *m_Realloc(void *self, void *p, size_t n, uint32_t a) {
    (void)self; void *q = realloc_aligned(p, n, a);
    if (!q && n) die("Unreal heap: out of memory in Realloc (%zu bytes, alignment %u, %.2f GB in use)", n, a, atomic_load(&g_used) / 1073741824.0);
    return q;
}
static void *m_TryRealloc(void *self, void *p, size_t n, uint32_t a) { (void)self; return realloc_aligned(p, n, a); }
static void m_Free(void *self, void *p) { (void)self; release(p); }
static size_t m_QuantizeSize(void *self, size_t n, uint32_t a) { (void)self; (void)a; return malloc_good_size(n); }
static bool m_GetAllocationSize(void *self, void *p, size_t *out) { (void)self; if (!p) return false; *out = malloc_size(p); return true; }
static void m_Nothing(void *self) { (void)self; }   // Trim (its bool argument is ignored), the thread-local cache setup and teardown, DumpAllocatorStats
static void *g_vt[NSLOT];
static const char *g_what;

// Observation gate OBS_UE_MALLOC: the allocator object GCreateMalloc just made (x19), before anything calls it
static void obj_made(uint64_t *regs, int param) {
    (void)param;
    *(void **)(uintptr_t)regs[19] = g_vt;
    static int once; if (!once++) LOG("%s: Unreal's allocator (FMallocBinned3, made at run time at %#llx) runs on a host malloc zone", g_what, (unsigned long long)regs[19]);
}

// Whether [a, a + n) may be Unreal heap memory: with the host allocator installed, a range outside the guest block that the host has mapped readable and writable.
// Unreal allocates the stacks of the threads it creates with FMemory (OCTOPATH TRAVELER II's game thread: nnMain at main+0x544 mallocs it with 4 KB alignment; its
// FRunnableThreads take theirs from inside larger allocations, so a stack is not always a block start, and malloc has no cheap lookup from an interior pointer), and
// nn::os::CreateThread moves such a stack into the stack region with svcMapMemory, whose source must be backed memory (hle_svc.c map_source_backed): this heap lives
// outside the guest block, so the check refused it and the SDK aborted in MapAliasStack before the game started (the caller has already checked the guest block's own regions)
int dm_ue_heap_owns(uint64_t a, uint64_t n) {
    if (!g_zone || !a || !n) return 0;
    int dm_guest_as_contains(uintptr_t a);   // hle_svc.c
    if (dm_guest_as_contains((uintptr_t)a) || dm_guest_as_contains((uintptr_t)(a + n - 1))) return 0;   // inside the guest block: not this heap
    vm_address_t at = (vm_address_t)a; vm_size_t sz = 0; vm_region_basic_info_data_64_t info; mach_msg_type_number_t cnt = VM_REGION_BASIC_INFO_COUNT_64; mach_port_t obj;
    if (vm_region_64(mach_task_self(), &at, &sz, VM_REGION_BASIC_INFO_64, (vm_region_info_t)&info, &cnt, &obj) != KERN_SUCCESS) return 0;   // vm_region_64: iOS has no mach_vm.h
    return at <= a && a + n <= at + sz && (info.protection & (VM_PROT_READ | VM_PROT_WRITE)) == (VM_PROT_READ | VM_PROT_WRITE);
}

// main+0xb0648 is `add x0, x21, #1` on the null-argument path, after x21 was built as 0x97fffffff.
// The reserved length and the region base are already stored at main+0x997e1e0 and main+0x997e1d8.
// main+0x14514 has not run yet, so the qword at main+0x98fc048 is still its relocated initial value.
static void ue_span_gate(uint64_t *regs, int param) {
    (void)param;
    // The gate is planted only for main c6b32f4d3d4243ff190b082b4f6aea18 (that profile's patches.py).
    module_t *m = NULL;
    for (int i = 0; i < g_nmod; i++) if (!strcmp(g_mod[i].name, "main")) { m = &g_mod[i]; break; }
    if (!m || m->size < 0x98fc088ull) return;
    uint64_t reserved = *(uint64_t *)(m->base + 0x997e1e0);
    uint64_t base = *(uint64_t *)(m->base + 0x997e1d8);
    uint64_t sentinel = *(uint64_t *)(m->base + 0x98fc048);
    uint64_t field_a = *(uint64_t *)(m->base + 0x98fc078);
    uint64_t field_b = *(uint64_t *)(m->base + 0x98fc080);
    uint64_t x21 = regs[21];
    if (!dm_ue_span_x21(reserved, base, sentinel, field_a, field_b, &x21)) return;
    regs[21] = x21;
    LOG("[mem] span reserved %#llx, null-path length %#llx, tail %#llx (second %#llx, third %#llx, refill %#llx)",
        (unsigned long long)reserved, (unsigned long long)(x21 + 1),
        (unsigned long long)(reserved - (x21 + 1)), (unsigned long long)DM_UE_SPAN_SECOND,
        (unsigned long long)(reserved - (x21 + 1) - DM_UE_SPAN_SECOND - DM_UE_SPAN_MIN),
        (unsigned long long)DM_UE_SPAN_MIN);
}

// main c6b32f4d3d4243ff190b082b4f6aea18 calls nn::os::AllocateMemoryPages through the PLT at
// main+0x7388650 (GOT main+0x98fa240). sdk+0x144840 loads the manager from [sdk+0xb68db8] and adds 0x420.
// The two commits in the DT_INIT caller return to main+0x291d8 and main+0x29280; x19 there is the commit
// object, [x19+0x20] its base, [x19+0x30] its stride, and w25/w26 the page indices. The global at
// main+0x9dcc920 is the pool base that caller adds a group shifted by 29 to. Which tree check returns
// 0xe03 depends on that base, those indices and the nodes actually registered. This records them and
// returns the guest result. It does not add a region and it does not turn 0xe03 into success.
enum { UE_PAGES_CAP = 16 };
static uint32_t (*g_pages_orig)(uint64_t addr, uint64_t size);
static int pages_rd(uint64_t a, void *o, size_t n) {
    vm_size_t g = 0;
    return a && !vm_read_overwrite(mach_task_self(), (vm_address_t)a, n, (vm_address_t)o, &g) && g == n;
}
int dm_addr_region_at(int i, uint64_t *base, uint64_t *size);
static void pages_loc(uint64_t a, char *b, size_t n) {
    module_t *m = module_of((void *)(uintptr_t)a);
    if (m) snprintf(b, n, "%s+%#llx", m->name, (unsigned long long)(a - (uint64_t)(uintptr_t)m->base));
    else snprintf(b, n, "%#llx", (unsigned long long)a);
}
static int pages_tree(module_t *sdk, dm_ue_page_node *node, int *root, int *trunc) {
    struct { uint64_t left, right, parent, pad, start, length; } raw;
    struct { uint64_t ptr; } got[UE_PAGES_CAP];
    uint64_t slot = 0, mgr = 0, hdr = 0, ptr = 0, stack[UE_PAGES_CAP];
    int n = 0, sp = 0;
    *root = -1; *trunc = 0;
    _Static_assert(sizeof raw == 0x30, "Vamm node links and the start/length pair");
    if (!pages_rd((uint64_t)(uintptr_t)sdk->base + 0xb68db8, &slot, 8) || !slot) { *trunc = 1; return 0; }
    mgr = slot + 0x420;
    if (!pages_rd(mgr + 0x40, &hdr, 8) || !hdr || !pages_rd(hdr + 0x10, &ptr, 8)) { *trunc = 1; return 0; }
    if (!ptr) return 0;
    stack[sp++] = ptr;
    while (sp) {
        uint64_t p = stack[--sp], link[3];
        int seen = 0, i;
        for (i = 0; i < n; i++) if (got[i].ptr == p) { seen = 1; break; }
        if (seen) continue;
        if (n == UE_PAGES_CAP) { *trunc = 1; break; }
        if (!pages_rd(p, &raw, sizeof raw)) { *trunc = 1; break; }
        got[n].ptr = p;
        node[n].start = raw.start; node[n].length = raw.length;
        node[n].left = node[n].right = node[n].parent = -1;
        link[0] = raw.left; link[1] = raw.right; link[2] = raw.parent;
        n++;
        for (i = 0; i < 3; i++) if (link[i]) {
            if (sp == UE_PAGES_CAP) { *trunc = 1; continue; }
            stack[sp++] = link[i];
        }
    }
    for (int i = 0; i < n; i++) if (got[i].ptr == ptr) *root = i;
    for (int i = 0; i < n; i++) {
        uint64_t l = 0, r = 0, p = 0;
        if (!pages_rd(got[i].ptr, &raw, sizeof raw)) { *trunc = 1; break; }
        l = raw.left; r = raw.right; p = raw.parent;
        for (int k = 0; k < n; k++) {
            if (l && got[k].ptr == l) node[i].left = k;
            if (r && got[k].ptr == r) node[i].right = k;
            if (p && got[k].ptr == p) node[i].parent = k;
        }
        if ((l && node[i].left < 0) || (r && node[i].right < 0) || (p && node[i].parent < 0)) *trunc = 1;
    }
    return n;
}
__attribute__((used)) uint32_t dm_ue_pages_from_guest(uint64_t addr, uint64_t size, uint64_t x19, uint64_t x25, uint64_t x26, uint64_t lr) {
    uint32_t result = g_pages_orig(addr, size);
    static _Atomic int logged;
    if (result != 0xe03 || atomic_fetch_add(&logged, 1) >= 4) return result;
    module_t *main_m = NULL, *sdk = NULL;
    for (int i = 0; i < g_nmod; i++) {
        if (!strcmp(g_mod[i].name, "main")) main_m = &g_mod[i];
        if (!strcmp(g_mod[i].name, "sdk")) sdk = &g_mod[i];
    }
    char where[48]; pages_loc(lr, where, sizeof where);
    uint64_t off = main_m && lr >= (uint64_t)(uintptr_t)main_m->base ? lr - (uint64_t)(uintptr_t)main_m->base : ~0ull;
    int known = off == 0x291d8 || off == 0x29280;
    uint64_t B = 0, P = 0, T = 0, composed = 0, bytes = 0;
    int have = 0, T_ok = 0;
    if (known) have = pages_rd(x19 + 0x20, &B, 8) && pages_rd(x19 + 0x30, &P, 8);
    if (main_m) T_ok = pages_rd((uint64_t)(uintptr_t)main_m->base + 0x9dcc920, &T, 8);
    if (have) {
        uint32_t cnt = ((uint32_t)x26 - (uint32_t)x25) + 1u;
        composed = B + (uint64_t)(uint32_t)x25 * P;
        bytes = (uint64_t)cnt * P;
    }
    dm_ue_page_node node[UE_PAGES_CAP];
    int root = -1, trunc = 0, n = 0, branch = DM_UE_PAGES_BROKEN;
    if (sdk) n = pages_tree(sdk, node, &root, &trunc);
    if (!trunc && sdk) branch = dm_ue_pages_classify(node, n, root, addr, size);
    LOG("[mem] pages e03 addr=%#llx size=%#llx lr=%s x19=%#llx B=%#llx P=%#llx i=%#llx j=%#llx composed=%#llx match=%d bytes=%#llx size_match=%d obj=%d T=%#llx T_ok=%d branch=%s%s",
        (unsigned long long)addr, (unsigned long long)size, where, (unsigned long long)x19,
        (unsigned long long)B, (unsigned long long)P, (unsigned long long)x25, (unsigned long long)x26,
        (unsigned long long)composed, have && composed == addr, (unsigned long long)bytes, have && bytes == size, have,
        (unsigned long long)T, T_ok, trunc ? "truncated" : (sdk ? dm_ue_pages_name(branch) : "unread"),
        known ? "" : " (x19 is not the commit object)");
    char line[1600]; size_t o = 0; int nr = dm_addr_region_at(0, NULL, NULL);
    o += (size_t)snprintf(line + o, sizeof line - o, "%d", nr);
    for (int i = 0; i < nr && i < 8 && o + 48 < sizeof line; i++) {
        uint64_t b = 0, sz = 0; dm_addr_region_at(i, &b, &sz);
        o += (size_t)snprintf(line + o, sizeof line - o, " %#llx+%#llx", (unsigned long long)b, (unsigned long long)sz);
    }
    LOG("[mem] pages regions %s", line);
    o = 0;
    o += (size_t)snprintf(line + o, sizeof line - o, "root=%d n=%d", root, n);
    for (int i = 0; i < n && o + 80 < sizeof line; i++)
        o += (size_t)snprintf(line + o, sizeof line - o, " %d:%#llx+%#llx L%d R%d P%d", i,
            (unsigned long long)node[i].start, (unsigned long long)node[i].length, node[i].left, node[i].right, node[i].parent);
    LOG("[mem] pages nodes %s", line);
    return result;
}
__attribute__((naked)) static void ue_pages_entry(void) {
    __asm__ volatile(
        "stp x29, x30, [sp, #-48]!\n\t"
        "mov x2, x19\n\t"
        "mov x3, x25\n\t"
        "mov x4, x26\n\t"
        "mov x5, x30\n\t"
        "bl _dm_ue_pages_from_guest\n\t"
        "ldp x29, x30, [sp], #48\n\t"
        "ret\n\t");
}
static void ue_pages_install(void) {
    static const uint32_t plt[4] = { 0xD0012B90u, 0xF9412211u, 0x91090210u, 0xD61F0220u };
    module_t *m = NULL, *sdk = NULL;
    for (int i = 0; i < g_nmod; i++) {
        if (!strcmp(g_mod[i].name, "main")) m = &g_mod[i];
        if (!strcmp(g_mod[i].name, "sdk")) sdk = &g_mod[i];
    }
    if (!m || strcmp(m->build_id, "c6b32f4d3d4243ff190b082b4f6aea18") || !sdk) return;
    if (m->size < 0x98fa248ull || sdk->size < 0xb68dc0ull || memcmp(m->base + 0x7388650, plt, sizeof plt)) {
        LOG("[RED] Dungeons II: AllocateMemoryPages PLT at main+0x7388650 is not the expected stub; the e03 log is not installed");
        return;
    }
    uint64_t *slot = (uint64_t *)(m->base + 0x98fa240), cur = *slot, sb = (uint64_t)(uintptr_t)sdk->base;
    if (cur < sb || cur >= sb + sdk->size) {
        LOG("[RED] Dungeons II: AllocateMemoryPages GOT main+0x98fa240 is not in sdk (%#llx); the e03 log is not installed", (unsigned long long)cur);
        return;
    }
    g_pages_orig = (uint32_t (*)(uint64_t, uint64_t))(uintptr_t)cur;
    *slot = (uint64_t)(uintptr_t)(void *)ue_pages_entry;
    LOG("[mem] Dungeons II: AllocateMemoryPages logs 0xe03 and returns the guest result");
}

// Called once the modules are loaded and relocated, before any guest code runs
void dm_ue_malloc_install(void) {
    ue_pages_install();
    gate_obs_set(OBS_UE_SPAN, ue_span_gate);
    for (int i = 0; i < g_nmod; i++) if (!strcmp(g_mod[i].name, "main"))
        for (int k = 0; k < (int)(sizeof k_ue / sizeof k_ue[0]); k++) if (!strcmp(g_mod[i].build_id, k_ue[k].build)) {
            uint8_t *b = g_mod[i].base; int L = k_ue[k].layout;
            if (*(uint32_t *)(b + k_ue[k].cut) != 0xd503201f) { LOG("[RED] %s: main is rewritten without the allocator cut (its patches.py): Unreal's own FMallocBinned3 runs", k_ue[k].what); return; }
            if (!k_ue[k].obj && !gate_obs_present(OBS_UE_MALLOC)) { LOG("[RED] %s: main is rewritten without the allocator gate (OBS of its patches.py): the allocator would have no vtable", k_ue[k].what); return; }
            memcpy(g_vt, b + k_ue[k].vtable, sizeof g_vt[0] * k_lay[L].nslot);   // the relocated vtable: the trivial slots stay the guest's
            g_vt[k_lay[L].malloc_] = (void *)m_Malloc; g_vt[k_lay[L].try_malloc] = (void *)m_TryMalloc; g_vt[k_lay[L].realloc_] = (void *)m_Realloc;
            g_vt[k_lay[L].try_realloc] = (void *)m_TryRealloc; g_vt[k_lay[L].free_] = (void *)m_Free;
            g_vt[k_lay[L].quantize] = (void *)m_QuantizeSize; g_vt[k_lay[L].get_size] = (void *)m_GetAllocationSize;
            for (int s = 0; s < 6 && k_lay[L].nothing[s]; s++) g_vt[k_lay[L].nothing[s]] = (void *)m_Nothing;
            g_zone = malloc_create_zone(0, 0); malloc_set_zone_name(g_zone, "guest Unreal heap"); g_what = k_ue[k].what;
            if (!k_ue[k].obj) { gate_obs_set(OBS_UE_MALLOC, obj_made); continue; }
            *(void **)(b + k_ue[k].obj) = g_vt;
            LOG("%s: Unreal's allocator (FMallocBinned3 at main+%#llx) runs on a host malloc zone", k_ue[k].what, (unsigned long long)k_ue[k].obj);
        }
}
