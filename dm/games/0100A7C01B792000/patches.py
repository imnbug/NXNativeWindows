# Minecraft Dungeons II 1.1.3.0 main. The null-argument path at +0xb0648 does
# add x0, x21, #1 after x21 was built as 0x97fffffff, requesting 0x980000000.
# The arena (main+0x28a74) is only the reserved region. The gate writes x21 =
# reserved length − 0x10000 − 1 so the add yields that length. b.hi is strict,
# and 0x10000 (the value stored at the arena object +0x48) stays free for one
# later minimum bump. No other build id is listed.
OBS = {
    'c6b32f4d3d4243ff190b082b4f6aea18': [
        (0xb0648, 26, '910006a0', 'null-path add x0, x21, #1: x21 becomes reserved length − 0x10000 − 1 before the add'),
    ],
}

GENERALIZE = 'No. The 38 GiB immediate, the arena fields and the length-1 add are this main. Other Unreal games use FMallocBinned3 and hle_uemalloc.c.'
