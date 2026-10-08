# Deep search — quantified findings (research only, no code changed)

Second pass over all 10 components. Corrections to first-pass notes are marked
FIX. All file:line verified.

## D1. Display/panel
- Active panel `nt36672a_auo_lm36273` (1080x2340, PLL540, VFP12) has NO dynamic
  table; siblings CSOT/Tianma (same NT36672A DDIC) prove VFP-only ARR
  60/40/30 (VFP 12/1115/2376, inform_lcm=0, DDIC cmds #if 0) —
  `csot_fhd_nt36672a_dsi_vdo.c:171-175,490-530`. Port to AUO = copy ARR block;
  only risk is flicker/VCOM at VFP2376 on AUO cell (unvalidated). hx83112b adds
  50fps level; nt36672c_90hz is HFR 60↔90 (different CPHY/geometry, not portable).
  j22 DRM panels all single 60Hz mode. 90/120/144Hz OC: infeasible.
- HBM (non-MT6853 branch, begonia): L1=1913/22.0mA, L2=1925/25.3mA (+15%),
  L3=1987/27.5mA (+8.7% over L2, +25% over L1, 92% of 30mA abs max).
  Norm max 1879@2047, so L3 is only +108 codes over normal max. Bias ±5.5V,
  sequencing CONF1 0x9c→0x9e on / reverse off (`leds-lm36273.c:91-133`).
  Recommend cap L2 until L3 thermal/chromaticity measured on AUO cell.

## D2. Memory backports (quantified)
- MGLRU upstream 6.1: 39 files, ~4073+/154-, conflicts SEVERE on 4.14 (no folio
  era, no lru_gen, vmscan 4281 + memcontrol 6420 lines). Last, after tunables.
- DAMON 5.15: ~2500-3500 lines new mm/damon/, HIGH conflict. Alternative:
  cold-first preset, not full DAMON.
- FIX: `memory.reclaim` per-memcg proactive reclaim is ALREADY backported here
  (~24-line handler `mm/memcontrol.c:5477-5500`, first-gen byte-count only) —
  wire userspace CachedAppOptimizer now, no kernel work.
- KSM: `mm/ksm.c` complete (3150 lines), defaults scan100/sleep20/sharing256;
  only missing `CONFIG_KSM` on begonia (defconfig flip + daemon).
- PSI=y enabled by default; hooks in vmscan + page_alloc present. lmkd props
  live in device tree, not kernel (zero ro.lmk in repo — expected).
- Order: reclaim → PSI-lmkd props → ZRAM streams/algo + idle-writeback sweep →
  swappiness/cluster/watermarks → memcg HWM + compressor-fullness kill (zram
  mm_stat + PSI-full, hysteresis) → KSM scoped → dentry valve → cold-first →
  MGLRU last.

## D3. Scheduler (quantified inventory)
- CFS: latency 4ms (`fair.c:59`), min/wakeup gran 500us (`:95,118`), margin 1280
  (`:160`), migration_cost 250us, sync_hint 1. Margin call sites
  `eas_plus.c:881,106-107,947`, `schedutil.c:232`.
- Schedtune: hold 50ms (`tune.c:18`), 10 boostgroups, root 0/0, write 0-100,
  eas_ctrl clamp ±100, stune_task_threshold dynamic (little cap).
- FBT defaults: bhr5, rescue 33%/var40, floor_opp2, loading_th=0 (light path
  DISABLED), sampling 256ms, idleprefer on, cap-margin on, TA masks
  LITTLE=all-6/7 BIG={6,7}. PPM Cobra 16 OPPs, budgets 3000/3000/3000/2500mW.
  Touch: floors 1.50/1.53GHz + TA 60%, 80ms hold, FIFO-90/98. utch TIME_TO_LAST
  600000 unit-mismatch flagged.
- Borrowed values: interaction +10 top-app auto-reset (DSB lineage), Kirisakura
  arm (top 0 + FG prefer_idle), Pixel touch ceiling 50 / static sweet 2-10
  (tree TOUCH_BOOST 80 runs hotter), OnePlus render→big affinity (path exists
  `fbt_cpu.c:1022`, gated PREFER_NONE), EEVDF→CFS translation (wakeup_gran
  500→250us proxy, no port).
- Measure first (userspace-only): top-app boost 0→10 / 50-gated, FG prefer_idle,
  touch 80→50 + hold 80→120ms; then sysctls (margin ±10%, wakeup_gran);
  then FBT policy (loading_th, rescue, PREFER_BIG) with 15-min thermal soak.

## D4. Battery/gauge/charging (quantified)
- Begonia JEITA overrides (`begonia-mt6785.dtsi:163-187`): T4 50→60/58,
  T3rel 39→43, T2 10/16→15/13, T1 0/6→5/2, CVs above-T2 4090mV / below 4390mV.
  Runtime FCC: T3-T4 2.20A, normal 1.80A, T1-T2 1.32A, T0-T1 0.44A
  (`mtk_charger.c:1096-1222`), dual-chg only in normal window.
- TA dual totals: 3.5A charge (1.7+1.8), 2.7A input (1.3+1.4); PE40 cap 9V
  (vs vanilla 11V); PD upper 9V DT; stop SOCs PE/PE20/PD 85/85/80.
- Cycle CV exists but coarse: 0-99→4390, 100-199→4370, 200-299→4350,
  300-2999→4330mV (`begonia-mt6785.dtsi:205-208`) vs Pixel 200→1000 gradual
  derate. `MI_CYCLE_COUNT_MAX 3999` begonia-only.
- Gauge gaps: CYCLE_COUNT reads `gm.bat_cycle` but kernel only accumulates ncar —
  persistence needs FG daemon (BAT_EC 785/786); CHARGE_FULL_DESIGN hardcoded
  4500000uAh vs per-ID q_max (SOH numerator/denominator diverge); no SOC
  hysteresis cap (Samsung 100/95, 80→78) — only HV stop gates.
- FIX: DT2W "CAP-gated" claim is FALSE. `double_click.c:1-60`: mode 0664, store
  is bare kstrtoint→set, NO capable()/uid check; protection is fs-mode only
  (build.sh chmods 0644). Tighten to 0644 + capable() as drive-by.
- Web anchors match local: PE 7/9/12V fixed, PE20 5-20V pattern, PE40 APDO;
  ROG watt-cap and Samsung/Adaptive schedules have no in-kernel equivalent —
  thermal-mitigation DT tables (16 levels) are the closest actuator.

## D5. Connectivity (quantified)
- BBRv1 only, 970 lines (`tcp_bbr.c`); v3 ≈ 2900-3300 lines (~+2000, 3x) touching
  tcp/tcp_minisocks/tcp_output — DEFER, confirms master decision.
- FIX/CONFIRMED: FQ skew is load-bearing. user defconfig FQ/FQ_CODEL NOT SET
  (`:1064-1065`, DEFAULT unset `:1070` → pfifo_fast) vs apatch FQ=y + DEFAULT fq.
  BBR pacing note (`tcp_bbr.c:55-57`, needs fq) violated on user builds. Fix =
  enable FQ in user defconfig or default_qdisc=fq via init (order-first).
- FASTOPEN not compiled (no CONFIG in either defconfig). RPS/RFS/XPS =y both —
  RX pin is script-only.
- Power-save: single FW gate CMD_ID_POWER_SAVE_MODE, unanimity required
  (`nic.c:2007-2037,2094-2111`); cfg80211 set_power_mgmt refuses PSP when
  !TIM (`gl_cfg80211.c:2050-2054`); no-TIM→CAM sticky (`scan.c:2707-2733`).
  U-APSD effectively off (bitmap 0, Tspec disabled) — VO/VI-in-game = set
  bitmap + CAM fallback + per-BSS denylist.
- Coex: host enables EXT_CMD 0x19 (`wlan_lib.c:490`); no host bias knob —
  gaming WiFi-bias is FW-blob A/B only.
- A/B plan: fq vs pfifo_fast (tc -s qdisc proof) × CAM vs PSP; metrics ping
  p99 (-5-30ms bar), iperf goodput/retrans/RTT-var, 30-min power, walk-test
  roam stalls, top-5-AP U-APSD interop (watch TIM-absence dmesg).

## D6. Camera (quantified costs)
- GW1 init table 3200 regs (12.8KB, 13 bursts @255) vs mode tables ~250 regs —
  init dominates ~12.5x. At 400kHz ≈ 290ms wire + mdelay(8); 1MHz ≈ 115ms
  (i2c_speed 1000 commented out). Stream enable fixed 10ms (5+5); stream-off
  poll ≤ ~334 iters; 3 dead frames per mode switch (2-frame AE).
- ISP/DISP/VENC/VDEC OPPs (`mt6785.dts:3189-3212`): CAM/IMG 560/416(or364)/315,
  VENC 630/450/364, VDEC 624/416/312. One-floor saving ≈ 26-33%.
  VENC SMI boost +100% (op≥120/H265/4K) / +30% (op60); VDEC emi_bw formula +
  codec scales verified. Pox holds peak between frames (enc `:327-344`, dec
  `:333-346`); DRAM OPP0 peak vs default 16 (`fbt_cpu_platform.c:47-64`).
- No userspace MTK decode low-latency key on C2 path (no MTK vendor key in
  Moonlight list; legacy vdec-lowlatency has no C2 equivalent; MTK MAGT keys
  encode-only) — kernel VDEC warm floor is the only in-kernel call lever.
- Cheapest first: VDEC/VENC history-warm step-1 not step-0 (2 lines, ~30% BW);
  decay 750→250ms Extreme-only hold (1 line); whitelist launch boost to
  camera/HAL; ISP 560→416 preview-gated; SMI boost 100→50 non-4K H265;
  pre-roll/ZSL last. Validate with pm_qos+i2c ftrace, drop counts, drain per
  10 launches, severity distribution.

## D7. Audio/haptics (quantified envelopes + FIX)
- HP: all moves via headset_volume_ramp 1dB/≥600us; direct ZCD writes
  mute/park-only. Default ceiling +4dB, absolute +8dB opt-in; mute -40dB.
- FIX: mic 5-7 are NOT HW-reserved — 3-bit field (`mt6359.h:1806`) encodes 0-7;
  restriction is enum/policy (valid 0-4 = 0/6/12/18/24dB). Two live bugs:
  `ul_pga_set` off-by-one (`>ARRAY_SIZE` admits 5, `:1023`) and
  `pox_mic_gain_set` clamp 0..7 (`:6952`) — tighten both to 0..4. VOW forces 4.
- Speaker: flat (reg,val) blobs (87359: 4×20, 87519: 3×19) + fw retry + AGC/DRC
  (BATSAFE/BSTOVR/BSTVPR on 87519); AGC_bypass is debug-only; no thermal/
  excursion policy in driver — HAL/profile work.
- Vibrator: invalid codes 3,6,7,10,12 (0V entries); sustained ≤2.8V (idx9),
  3.0/3.3 bursts ≤50ms; <10ms rounds to 25ms; 15s cap; OC = PMIC-HW trip +
  stop-only handler (NO sw threshold, add cooldown + foldback).
- Torch: WDT 1248ms + strobe 400ms + PMU 64-2432/32ms-step (flash-only);
  POX timeout_ms=0 (infinite) is the outlier — fix 3-5min + 30s warn;
  step-down 24→15→9→off; camera yield (charger-HV path `:375-409` exists,
  wire to POX). Precedents: FP5/Sony auto-off, iOS 30s continuous cap (we are
  stricter at 15s HW).

## D8. System (design)
- Per-mode block sets (0/1/2/3) with exempt lists: never mask MD_WDT IRQ,
  D2H_EXCEPTION_INIT, CCCI STATUS/sysmsg, alarm/kpd/spm/lpm, SPM MD_WDT wake-src.
- Abort-rate loop on RO counters (success/fail, last_failed, wakeup_sources
  table, abort strings): Extreme→Deep >15%, Deep→Balanced >30% (20 attempts or
  10min window), step up only <5% twice; per-source >40% → shorten hold
  (HZ→HZ/2) not block. Freeze via CGROUP_FREEZER, cached/bg UIDs only, FG
  exempt, thaw on PM_POST_SUSPEND.
- Rails: never lengthen SCP_AWAKE_TIMEOUT; never enable AUTOSLEEP; DT2W fix as
  in D4; writable 0644+capable, RO 0444, no selinux/proc-core changes.
