# 04 Storage / I/O — component master (research only, no code changed)

UFS: CMD_PER_LUN/CAN_QUEUE 32 (`ufshcd.c:140`). Default sched: deadline
(begonia) vs cfq (stock). Banner claims BFQ but default is deadline — mismatch.

## Existing (file:line)
- Kconfig: NOOP/DEADLINE/CFQ/BFQ/KYBER built; `DEFAULT_DEADLINE`, default
  `"deadline"` (begonia) vs `"cfq"` (stock).
- BFQ `bfq-iosched.c:151` slice_idle=0 (upstream 8ms); CFQ `cfq-iosched.c:35`
  slice_idle=0; both already flash-tuned, low_latency default on (BFQ).
- deadline/mq-deadline stock timings: read 500ms / write 5000ms / starved 2 /
  batch 16 (`mq-deadline.c:29-32`).
- Ramdisk (`build.sh`): readahead 512 boot/perf vs 128 balanced/powersave;
  rq_affinity 2, iostats 0, add_random 0, nomerges 1, nr_requests 256 FIXED all
  modes; NO scheduler write in ramdisk.
- Dynamic fsync `fs/sync.c:25-38,241-242`: default OFF, S_ISREG-only skip,
  sysfs 0644 CAP-gated + proc mirror; no auto-link to gaming_mode.
- Dirty: kernel 5/10, 500/3000cs; ramdisk 10/5, 1500/300 single table, no
  per-mode split, no thermal derate.
- F2FS options: nothing in-tree (no fstab, no mount writes); kernel supports
  background_gc/discard/flush_merge via `fs/f2fs/super.c`, sysfs GC/discard
  knobs — ROM-fstab dependent.

## Borrowed ideas (ramdisk/sysfs only)
1. Scheduler triple: gaming deadline / balanced bfq / powersave noop+hibern8 —
   EASY echo, fallback to deadline.
2. mq-deadline gaming tune: read 500→250, write 5000→1500, batch 16→8 —
   EASY; keep camera profile stock (sequential writes).
3. Readahead/queue pairs per mode (512/256 game, 256/128 balanced, 128/64 saver;
   rq_affinity 2→0 saver) — EASY; verify boot device sda vs mmcblk0.
4. nomerges 1 gaming / 0 otherwise (merge CPU on small random writes) — EASY.
5. Per-mode dirty tables + thermal derate to balanced on skin-temp trip (ROG
   lesson) — EASY.
6. Dynamic-fsync never-off rule: allow ONLY gaming=1 + charger/bypass, auto-clear
   on exit/screen-off/saver/thermal + skip-seconds counter (iOS durability
   lesson: never for DB/WAL) — EASY policy.
7. F2FS fsync/flush_merge/background_gc preset IF userdata is F2FS (check ROM
   fstab first; never nobarrier) — EASY-if-F2FS else NO-OP.
8. GC/discard idle-only + batching, health-gated (JEDEC/Pixel lesson: continuous
   discard wears NAND) — EASY if idle-gated.
9. UFS link policy: aggressive auto_hibern8 saver, relaxed gaming (exit latency)
   — EASY-MOD.
10. Sustained-write cooling tie-in (temp trip → balanced tables + write_expire
    restore + cap bg GC) — EASY.

Order: #3+#5 → #1+#2+#4 → #6 guard → #8+#9+#10 → #7 last.
