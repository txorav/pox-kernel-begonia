# 01 CPU + Scheduler — component master (research only, no code changed)

SoC: MT6785 (2x A76 + 6x A55). Base: 4.14 CFS + EAS + schedtune/uclamp + MTK FPSGO.

## Existing (file:line)
- CFS: `kernel/sched/fair.c:59-60` latency 4ms, `:95-96` min_gran 500us,
  `:118-119` wakeup_gran 500us, `:160` capacity_margin 1280.
  Features `kernel/sched/features.h`: ENERGY_AWARE, FIND_BEST_TARGET, UTIL_EST on.
- Margin runtime: `kernel/sched/core.c:1911-1921` set/get_capacity_margin.
- schedtune/uclamp: `kernel/sched/tune.c` (boost/prefer_idle), `tune_plus.c:147,189`
  boost_write/prefer_idle_for_perf_idx; defconfig UCLAMP_TASK/GROUP + MAP_OPP on,
  WALT off, PELT halflife 32.
- schedutil: `kernel/sched/cpufreq_schedutil.c:35` up/down_rate limits,
  `:588,628` exported setters; MTK variant `cpufreq_schedutil_plus.c:19`.
  HZ=300, PREEMPT=y, SCHED_HRTICK=y.
- MTK: `eas_plus.c` energy placement, `sched_ctl.c:491-505` uclamp-aware util,
  PPM COBRA caps (`mtk_ppm_cobra_algo.c`, `mtk_ppm_main.c`), FPSGO fbt_cpu
  (`fbt/src/fbt_cpu.c`, `fbt_cpu_platform.c:70` TA uclamp), xgf render tracker,
  uboost vsync timer, `eas_ctrl.c:432` max-wins uclamp, tchbst touch boost
  (`tchbst/kernel/ktch.c:66-94`: 1.50/1.53GHz floors + 60% TA uclamp, 80ms hold,
  FIFO-90/98 threads). task_turbo DISABLED (`defconfig:1888` not set).
- DTS note: `mt6785.dts:46-130` big cores labelled A75 (stale string, A76 silicon).

## Borrowed ideas (feasibility / gain / risk)
1. Tighten schedutil up_rate on big (500→200us), keep down_rate ≥5-10ms — EASY,
   faster frame ramp; gate with PPM thermal.
2. capacity_margin 1280→1152-1216 in gaming only — EASY, sooner big-core use;
   restore on exit (watch overutilized flapping `fair.c:8066`).
3. CFS gaming sysctls: latency 4→3ms, wakeup_gran 0.5→0.25ms — EASY, render
   wake-preempt; scope to game sessions (Sultan lesson: small deltas + measure).
4. Extend touch boost hold 80→120ms in game — EASY, smoother tap→frame.
5. FPSGO per-title TA floor via fbt_set_boost_value + per-task min_cap — EASY-MOD.
6. EAS_PREFER_IDLE for TA + FIND_BEST_TARGET — MOD, better idle-big placement.
7. QoS bands via uclamp groups (iOS-style: TA/FG floor, bg cap, never cap
   SF/system_server) — MOD.
8. PPM COBRA game profile (raise budget/pin big min OPP while game) — MOD,
   respect THERMAL/PTPOD priority.
9. Heavy-task rotation/migration thresholds so render stays on big — MOD.
10. Shorten schedtune boost hold / selective BOOST_HOLD_ALL — MOD, less stale freq.
11. Do NOT backport EEVDF/sched_ext; do NOT flip PELT/WALT/HMP — decision, avoids
    destabilizing proven stack (emulate EEVDF with #3/#6).
12. Keep kernel/sched at -O2, no LTO (Kirisakura restraint) — EASY.

Order: #1+#4+#11+#12 (zero-risk) → #2+#3+#5 behind gaming toggle → #6-#10 one by
one with fps + sched_stat + thermal traces.
