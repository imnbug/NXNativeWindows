# Minecraft Dungeons II 1.1.3.0 main. The null-argument path at +0xb0648 does
# add x0, x21, #1 after x21 was built as 0x97fffffff, requesting 0x980000000.
# The arena (main+0x28a74) is only the reserved region. This init then bumps
# 0x200000 (main+0xb0834) and a third length from main+0x14514 (main+0xb0a34).
# The gate writes x21 = reserved − those two bumps − the 0x10000 freelist
# refill at main+0x28b70 − 1. b.hi is strict. The gate word is unchanged.
# No other build id is listed.
OBS = {
    'c6b32f4d3d4243ff190b082b4f6aea18': [
        (0xb0648, 26, '910006a0', 'null-path add x0, x21, #1: x21 becomes reserved − the 0x200000 bump − the third bump − the 0x10000 freelist refill − 1'),
    ],
}

GENERALIZE = 'No. The 38 GiB immediate, the arena fields and the length-1 add are this main. Other Unreal games use FMallocBinned3 and hle_uemalloc.c.'
