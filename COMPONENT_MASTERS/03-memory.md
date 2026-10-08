# 03 Memory — component master (research only, no code changed)

RAM: 6GB class. Current bias: anon→ZRAM (swappiness 100), 2% kswapd headroom.

## Existing (file:line)
- `mm/vmscan.c:163` swappiness=100; classic LRU only (no MGLRU — zero hits).
- `mm/page_alloc.c:345-352` min_free 1024, watermark_scale_factor=200; PSI hooks
  on slowpath `:3514,3740`.
- `mm/swap.c:44` page_cluster; `fs/dcache.c:89` vfs_pressure=50.
- ZRAM `zram_drv.c:46` default zstd; streams/algorithm/disksize/writeback sysfs
  present (`:979,998,1720,623`); NO recompress attr. `CONFIG_ZRAM_WRITEBACK=y`,
  `ZSMALLOC_STAT=y`, `ZSWAP=y`, `CRYPTO_ZSTD/LZ4/LZO=y`.
- KSM: full `mm/ksm.c:251-278` tunable set present but `CONFIG_KSM` NOT SET on
  begonia configs (only generic/cuttlefish) — absent on shipping builds.
- LMK: in-kernel lowmemorykiller ABSENT (no staging driver); expect userspace
  lmkd + oom_score_adj (`kernel/fork.c` wiring only).
- PSI `=y` + enabled by default; MEMCG + MEMCG_SWAP `=y`. MGLRU absent. DAMON
  absent (`mm/damon/` missing). IDLE_PAGE_TRACKING=n, THP=n, COMPACTION=y.

## Borrowed ideas
1. PSI-driven lmkd tuning (partial ~70ms / full ~700ms per AOSP) — props only,
   kernel PSI ready. EASY, earlier cleaner kills.
2. Per-mode swappiness (100 interactive / 60-80 saver) + page-cluster pairs —
   EASY sysctl hooks.
3. Watermark/min_free per-mode (bigger headroom gaming/camera, smaller idle) —
   EASY; don't over-reserve 6GB.
4. ZRAM streams≈nr_cpu + algorithm per mode (lz4 latency vs zstd ratio) — EASY.
5. Idle writeback-to-swap sweep as manual recompress-tier (needs backing device
   + daemon, mmd-style) — MOD, emulates secondary tier; watch flash wear.
6. KSM scoped to shared anon (zygote), screen-off/idle scanning, ksmtuned-style
   rates — MOD (defconfig flip + daemon); CPU/burn guard.
7. Jetsam-style per-app HWM via memcg high (throttle) < max (kill) per standby
   bucket — MOD, contains runaways before global pressure.
8. Compressor-fullness early-kill (iOS `compressor-space` lesson: zram mm_stat +
   PSI-full → kill lowest cached before thrash, hysteresis-gated) — MOD.
9. Cached-app proactive reclaim on app-cached transition (CachedAppOptimizer
   pattern; `memory.reclaim` doesn't exist on 4.19, do by hand) — MOD.
10. Dentry pressure valve (raise vfs_pressure 50→150-200 under PSI-full) — EASY.
11. MGLRU backport — HIGH gain but big 4.19 patch + soak risk; do AFTER 1-9.
12. 3-profile glue (Performance/Balanced/Battery setting all of the above) — EASY.

Order: 1→4→2/3→8→7→9→5→6→10→12→11.
