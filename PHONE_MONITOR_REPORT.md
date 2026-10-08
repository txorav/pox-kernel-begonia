# Phone monitor report — adb live sample (no changes made)

Device: Redmi Note 8 Pro (begonia), Pox 4.14.357-Onyx, Android 16 SDK36.
Samples taken ~2-4 min after boot (boot storm still active — re-sample at
steady state before acting on performance numbers).

## Raw samples
- Kernel: `4.14.357-Pox-0.9-Onyx-onyx-449faa0d0`, Clang 11.0.1, SMP PREEMPT.
- Uptime/load: 0min load 26 → 2min load 41 → 3min load 34 (8 cores, ~4x
  oversubscribed; 786 tasks; us40/sy21, swap-in 94 + swap-out 1006, IO-read
  77k blocks in vmstat window).
- Top CPU: tencent QQ/MM, google quicksearchbox, smartisland, surfaceflinger,
  system_server, launcher, WeChat 151% single-thread.
- Top RSS: WeChat 795MB, Google app 674MB, system 515MB, QQ 500MB, FB 424MB,
  surfaceflinger 397MB, IG 399MB, ayugram 358MB, GMS 357MB, systemui 355MB.
- Memory: 7401/7654MB used (96.7%), free ~250MB, cached 3.5GB, swap used
  25→64→131MB across samples (zram absorbing, pool filling).
- Thermals (thermalservice HAL): CPU/GPU/NPU 69.4C, SKIN 47.8C, PA 42.7C,
  battery 25.5C. Status 0 (no framework throttle yet), cpu_adaptive_0 active.
- CPU freq: LITTLE 1666MHz (max ~1701), big 2050MHz (near max) — pinned high.
- gaming_mode: 1 ACTIVE by default (ultra_rescue, 20%/15, 500/20000us,
  GPU +20%, TA 20%, margin 1350, VIVID, DRAM peak, touch 1.5/1.53GHz+60%).
- Battery: 35%, USB powered, 3689mV, 593mA max, 25.7C, max-cap 4385900 /
  design 4500000 (~97.5% health — good).
- Access: shell uid2000, no su. gaming_mode readable; /proc/perfmgr listing,
  battery_protection, pressure/memory, zram, thermal_zone all denied for shell
  (SELinux enforcing for shell — expected, not a bug).

## Interpretation
1. Figures are BOOT STORM, not steady state: dozens of heavy apps (WeChat, QQ,
   FB, IG, ayugram, Gmail, Udemy, ProtonMail, Chrome) all autostarted at once.
   Load 34 + 69C CPU + 47.8C skin + pinned freqs + swap-out reflect dexopt/
   sync/startup surge amplified by gaming_mode=1 default-ON (perf-first during
   the worst possible window).
2. Memory is genuinely tight: 250MB free with 3.5GB reclaimable cache; zram
   climbing. If free stays <300MB at steady state with swap-out >0, background
   kills/jank will follow.
3. Skin 47.8C at 3 min post-boot is the actionable warning: sustained >45C
   skin is uncomfortable and accelerates aging; CPU 69C still below hard
   throttle but trajectory matters.

## Candidate optimizations (proposals only, map to roadmap)
- O1 (userspace, biggest): cut autostart (FB/IG/QQ/WeChat/ayugram/Gmail) +
  restrict background; re-sample steady-state load/free/swap before kernel
  changes. Maps to COMPONENT 10 §4 + ROADMAP §9.
- O2 (kernel policy): default gaming_mode=0 BALANCED, enter gaming only on
  game foreground (ROADMAP §1/§4, 01 §2-5). Would have kept boot at margin
  1280/TA 5% instead of 1350/20% + DRAM peak.
- O3 (memory): PSI-lmkd + per-mode swappiness/watermark + zram streams/algo
  (03 §1-4, DEEP D2). Validate with steady-state `free + swap-out + PSI`.
- O4 (thermal): balanced ISP/VDEC floors + decay 750→250ms + skin feedforward
  into charge/dirty (05 §1-3, 08 §8, 04 §10). Skin 47.8C is the trigger metric.
- O5 (net): FQ fix + gaming CAM only (09 §4/§1) — no evidence of net issue
  today; skip unless ping-tail shows bufferbloat.
- O6 (battery): health 97.5% — no action; consider 80% overnight cap per 08.

## Next sample (steady state, screen-on idle 10 min + screen-off 10 min)
- `uptime; free -m; cat /proc/loadavg` equivalent via free/vmstat (shell-safe).
- `dumpsys meminfo` top RSS + `dumpsys gfxinfo` jank (needs app focus).
- `dumpsys thermalservice` CPU/SKIN trend; `dumpsys battery` temp/level rate.
- Re-run after O1 to isolate kernel vs autostart contribution.
