# 02 GPU + Display — component master (research only, no code changed)

GPU: Mali-G76 MC4 (OPP0 900MHz → OPP42 270MHz, `mtk_gpufreq_core.h:20-62`).
Panel: 60Hz LCD (DTS fps 6000, PLL540 VDO, no dynamic_fps table on active LCM).

## Existing (file:line)
- GED DVFS `gpu/ged/src/ged_dvfs.c`: boost_gpu_enable=1, margin infra
  (`GED_DVFS_TIMER_BASED_DVFS_MARGIN 30`, gx_tb/fb margins), vsync-offset/GAS
  events, `ged_dvfs_set_gaming_boost()` (margin 520). KPI `ged_kpi.c`:
  gx_dfps/gx_game_mode/gx_boost_on/cpu_boost_policy tables; HAL `ged_hal.c`
  custom_boost/upbound sysfs; 3D-fence smart boost; SKI clamps.
- gaming_mode focuses strictly on compute/touch performance (GED DVFS, PPM COBRA,
  FPSGO, touch boost) and does NOT hijack display calibration.
- Color / True Tone: True Tone (Calibrated D65 Liquid Retina Reference, Mode 1) is the
  global system default display profile, exposed via `/proc/perfmgr/true_tone` and
  `/sys/kernel/true_tone`. Decoupled from gaming mode. Switchable to 4 modes via `color_mode`
  (0=STANDARD, 1=TRUE_TONE, 2=VIVID, 3=SLOG3).
- HBM: L1 22mA / L2 25.3mA / L3 27.5mA (`leds-lm36273.h:22-27`),
  `pox_lm36273_hbm_set(0-3)`, manual only, no ALS/timeout/thermal.
- Refresh: ARR infra exists (`primary_display.c`) but active `nt36672a_auo`
  panel has NO table → ARR off. Siblings show downshift-only 60→40→30.
  90/120/144Hz: no entries anywhere — infeasible on this hardware.

## Borrowed ideas
1. GED-idler (Adreno-Idler port): force bottom OPP after ~300-500ms idle util
   <20%, cancel on fence spike — HIGH feasibility, med battery gain.
2. Touch/interaction GPU pre-boost (<200ms GAS kick) — HIGH, perceived smoothness.
3. Per-app frame-rate-range hints (ProMotion-lite: idle 30 / game 60 arbiter over
   existing per-ID target FPS) — MED.
4. ARR static downshift 60→40→30 (Kirisakura-static analog) — MED, panel-gated;
   only for panel variants with tables.
5. HBM-auto via ALS (hysteresis + dwell + thermal/timeout cap) — HIGH.
6. KLapse-lite warm shift via COLOR/CCORR (kernel-side, no overlay) — MED.
7. Gaming fast-path preset (VIVID + AAL ease-off, per-game memory) — HIGH wiring.
8. Do NOT do MEMC/HyperRendering (no MEMC block, no Mali frame-gen hooks, 60Hz
   panel) — infeasible, would add latency/artifacts.
9. Do NOT do 90/120/144Hz panel OC (60-only LCM tables + PLL540) — very high
   brick risk.
10. DVFS-margin profiles Battery vs Perf via existing module params — HIGH.
11. Floor/ceiling guardrails (gpu_bottom_freq + custom_upbound cap) — HIGH.
12. Backlight low-end curve + warm-dim pairing — MED.

Order: 1→10→11→2→5→6 → 3→7→4. Reject 8, 9.
