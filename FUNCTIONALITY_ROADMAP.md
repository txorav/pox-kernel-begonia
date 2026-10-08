# Functionality roadmap — proposals only (no code changed)

Base: 4.14.357 MT6785 begonia, branch `onyx`. This file proposes what *could* be
added/tuned. Nothing here is implemented. Duplicates of existing work are
marked SKIP to avoid re-proposing.

## Already exists — do not re-propose (verified)
- `/proc/perfmgr/` nodes: gaming_mode, color_mode, camera_profile, camera_4k60,
  slog3, battery_bypass/limit/status, touch_game_mode/sensitivity, headphone_gain,
  vibrator_strength, wakelock_blocker, fast_charge, dt2w, mic_gain, dynamic_fsync,
  hbm_mode, torch/flashlight_brightness (`gaming_mode.c:1440-1522`, default ON `:1578`).
- FPSGO ultra-rescue + schedutil 500us + GED boost + PPM perf-first + SchedTune +
  EAS margin + DRAM boost (`gaming_mode.c:163-331`); camera launch boost + 750ms
  decay (`:91-131`); ISP 560MHz floor (`cam_qos.c:562-584`).
- Memory: swappiness 100 (`vmscan.c`), watermark 200 (`page_alloc.c`),
  page-cluster 0 (`swap.c`), vfs pressure 50 (`dcache.c`), dirty 10/5
  (`page-writeback.c` + ramdisk overrides), zram default zstd (`zram_drv.c:47`).
- Scheduler: latency 4ms, min_granularity/wakeup 500us, child_runs_first=1,
  capacity_margin default 1280 (`fair.c`, `core.c:1911`).
- I/O: BFQ/CFQ slice_idle=0, UFS readahead 512 + nr_requests 256 + nomerges
  (`build.sh:480-505`); TCP BBR available (`tcp_bbr.c`), fq + fastopen via ramdisk.
- Power/thermal: COBRA perf-first hint, screen-off cap Big 1.5GHz
  (`mtk_ppm_policy_lcm_off.c:72`), CLATM CFG1 MIN 1200/1000mW
  (`clatm_initcfg.h:37-40`), battery_guard.rc (limit 80, temp 39).
- Task-turbo launch/binder/sched boosting (`task_turbo.c:381-614`); wakelock
  blocker list (`wakeup.c:590-598`); DT2W (`double_click.c`); xiaomi touch modes;
  mt6359 hp/mic gains; lm36273 HBM L1-L3; dynamic fsync (`sync.c:236`).

## 1. CPU scheduler (tune, no new governor)
1.1 Per-mode schedtune presets (powersave/balanced/gaming/extreme tables for
top-app/foreground boosts + prefer_idle) instead of single default-ON gaming.
1.2 Schedutil rate-limit presets per cluster with thermal guard (restore 2000us
up-ramp when skin temp high; never pin 500us unconditionally).
1.3 EAS margin presets (e.g. 1126/1280/1350 already exist — expose as named
profiles, add energy-vs-perf curve for A55 packing).
1.4 Optional: sched group/heavy-task placement hint for RenderThread + camera ISP
threads (build on task_turbo cgroup boost, no new scheduler).

## 2. Memory (biggest user-visible win left)
2.1 KSM (kernel samepage merging) with app/ROM opt-in toggle — dedups Zygote
forks; needs `CONFIG_KSM=y` + ksmd scan tunables.
2.2 zram: multi-stream + recompression choice (zstd vs lz4/lz4hc per priority),
writeback-to-UFS toggle, smarter swappiness-per-mode (e.g. 60 balanced / 100
gaming) instead of fixed 100.
2.3 LMK/lmkd tuning presets + `oom_score_adj` profiles for launcher/camera/IME;
watermark_scale_factor per-mode (150 vs 200) rather than hardcoded 200.
2.4 `page-cluster` per-mode (0 gaming / 3 balanced) since 0 costs sequential
readahead; vfs_pressure per-mode (50 vs 100).
2.5 UKSM-style or zswap-style compressed swap cache evaluation (4.14-compatible
backports only; avoid experimental allocators).

## 3. Storage / I/O
3.1 I/O scheduler presets: deadline (gaming) / cfq/bfq (balanced) / noop-power
(powersave) with UFS queue depth + readahead pairs (128 vs 512), not fixed 512.
3.2 Fsync/dirty presets: dynamic_fsync already exists — add per-mode dirty
ratio/expire/interval tables + eMMC/UFS lifetime guard (never leave fsync off
in powersave/balanced).
3.3 F2FS/ext4 mount-option presets (if ROM uses F2FS): background_gc, discard,
compress extensions — via ramdisk, not kernel patch.

## 4. GPU / display / touch
4.1 GED presets: boost freq tables per game load (light/medium/extreme) + idle
hysteresis, instead of single boost_gpu_enable=1.
4.2 Frame-pacing assist: FPSGO rescue_percent/variance/BHR per-mode tables
(already parameterized — just add named presets + auto-decay to balanced).
4.3 Display: color-mode presets already exist (0-3) — add night/low-blue,
sRGB-clamp, per-app profile; HBM auto (lux-based) instead of manual 0-3 only.
4.4 Touch: sensitivity/game-mode already exist — add report-rate presets,
palm-reject toggle, stylus/glove mode if NT36672A supports it.

## 5. Camera / media
5.1 VENC/ISP QoS presets already at peak — add balanced floor (e.g. 300MHz)
vs peak 560MHz to save power when viewfinder idle.
5.2 Per-scenario fps caps (preview 30 / video 30-60 / slow-mo passthrough)
instead of global 4K60 force; keep launch boost but shorten decay when thermal high.
5.3 Codec: prefer hardware decode whitelist + low-latency decode flag for
video calls; no new codec driver needed.

## 6. Audio / haptics / LEDs
6.1 Sound: hp/mic gains exist — add per-device presets (speaker/headset/BT),
compander/DRC toggle, safe-volume ceiling that survives reboots.
6.2 Vibrator: strength exists — add waveform/effect presets + thermal ceiling
(never allow 3.3V sustained).
6.3 Torch/LED: brightness grading exists — add timeout + thermal step-down +
camera-use arbitration (return control to camera app after call).

## 7. Power / battery / charging
7.1 Charge-limit profiles (65/80/90/100) + bypass per-mode (gaming bypass ON,
balanced OFF); keep temp guard (39C) and add skin-temp feed-forward.
7.2 Fast-charge negotiation presets (USB-BC1.2 vs PD) with cable-impedance
derate; never force current beyond charger advertisement.
7.3 Screen-off caps already exist — add doze/deep-idle preset (cpu floor,
wlan scan backoff, sensor batching).

## 8. Connectivity (tune only, no new stack)
8.1 TCP: BBR default + fq already via ramdisk — add per-network presets
(WiFi BBR / cellular cubic fallback), keep ECN + syncookies.
8.2 WiFi: CAM vs power-save presets per gaming state (already toggled in
init.gaming.rc — formalize as driver preset, not shell echo).
8.3 BT coexistence: gaming low-jitter SCO/eSCO preset vs balanced.

## 9. Wake / sleep / background
9.1 Wakelock blocker list exists — convert hardcoded list to allowlist file +
per-mode sets (gaming blocks scan wakelocks, balanced does not).
9.2 Doze/ALS/proximity presets: pocket-mode (disable dt2w when prox covered)
already half-there — finish as named profile.

## 10. Observability (no perf cost by default)
10.1 One `/proc/perfmgr/profile` meta-node (read current mode + active preset
versions) so ROM scripts stop scraping 20 nodes.
10.2 Lightweight counters: frame-miss count, thermal throttle seconds, zram
savings, fsync-skip seconds — exposed read-only, off by default in powersave.
10.3 `dmesg`-quiet boot presets (already STOP_SHIP_TRACEPRINTK) + per-mode
loglevel.

## Suggested order (impact vs risk)
1. Memory presets (2.2-2.4) + scheduler presets (1.1-1.3) — biggest feel gain.
2. GPU/FPSGO preset tables (4.1-4.2) + camera balanced floor (5.1).
3. Charge/bypass profiles with guards (7.1-7.2).
4. I/O + TCP + WiFi presets (3.x, 8.x).
5. Observability (10.x) last; enables A/B testing of 1-4.

## Explicit non-goals (do not add)
- New governors/schedulers from scratch; new filesystems; custom OOM killer.
- Duplicate nodes for what exists above (see SKIP list).
- Any change that widens the A1/A2 attack surface in SECURITY_KNOWN_ISSUES.md
  (new nodes must be 0444/0644 + capability-checked, not 0666).

## 11. Borrowed ideas — other phones / kernels / OSes (proposals only)

Sources checked: Sultan/Kirisakura/ElementalX/Franco feature sets, Linux 6.x
(MGLRU, DAMON, EEVDF, sched_ext, BBRv3), ASUS ROG / OnePlus gaming phones,
Pixel + Samsung battery stacks, iOS Jetsam/compressor/ProMotion/haptics.
Each item notes 4.14 feasibility: EASY (tune/ramdisk), MODERATE (bounded
backport), HARD (needs newer kernel — spec only, do not attempt blindly).

### 11.1 From Sultan / Kirisakura / ElementalX / Franco
- 11.1.1 Sultan-style efficiency pass (MODERATE): disable unneeded debug
  configs in perf builds, keep Linux-stable 4.14.3xx backports flowing, prefer
  `mq-deadline` tuned defaults for UFS (Sultan ports tune mq-deadline +
  ondemand/conservative for efficiency). Do NOT copy "disable SELinux auditing"
  (Kirisakura does this for stock ROMs) — keeps denials invisible; bad trade.
- 11.1.2 MGLRU + PSI + MEMCG revival (MODERATE/HARD): Sultan Pixel ports bring
  back MGLRU/PSI/MEMCG and disable SLMK for better reclaim. On 4.14 this is a
  bounded backport; payoff is smoother multitasking than fixed swappiness=100.
  Pair with LMK driven by PSI (see 11.5.1) instead of static minfree.
- 11.1.3 Updated zstd/lz + BBRv3 default (MODERATE): Sultan updates zstd and
  enables BBRv3 by default. Begonia already defaults zstd zram — remaining work
  is crypto/compress library refresh + BBRv1→v3 congestion update and
  per-network default (WiFi BBR / cellular fallback).
- 11.1.4 CFI + ThinLTO + SCS + LLD/RELR (HARD on 4.14 MTK/Clang-11): Kirisakura
  enforces CFI (fixing QCOM/OEM violations found in permissive mode) with
  ThinLTO+SCS. Correct direction for security, but needs full-tree visibility
  and violation triage; spec as hardening track, not a weekend patch.
- 11.1.5 Static-image display power saving (EASY): Kirisakura adds display
  tweaks to cut power on static frames. Add panel self-refresh / low-fps idle
  preset when supported by the begonia panel.
- 11.1.6 Per-app profiles, FKM-style (EASY kernel side): Franco/EXKM let users
  set per-app CPU/GPU/IO + WiFi/location/battery-saver actions (e.g. max freq
  for games, low freq for e-books). Kernel work is only the preset + fast-switch
  hook (already have gaming_mode); policy lives in userspace manager.
- 11.1.7 Wake gestures beyond DT2W (EASY/MODERATE): ElementalX signature is
  sweep2wake/doubletap/sleep gestures. DT2W exists — add sweep-to-sleep and
  pocket-guard (prox sensor) before adding more gestures.
- 11.1.8 HBM auto by ambient light (EASY): Franco/EXKM do HBM + auto toggle on
  light sensor (Pixel 3/4 pattern). Begonia HBM is manual 0-3 — add lux-based
  auto step-up/step-down with timeout.
- 11.1.9 KLapse / Night Shift tint (EASY/MODERATE): Franco/ElementalX color
  temperature presets + timed orange/red tint. Color modes 0-3 exist — add warm
  preset + schedule hook, no new color pipeline.
- 11.1.10 GPU idler a la Adreno Idler (MODERATE): FKM supports Adreno Idler to
  clock-gate idle GPU. Mali-G76/GED equivalent: aggressive idle down-clock +
  hysteresis table per load band (extends 4.1).
- 11.1.11 CPU input-boost + stune-app Boost (EASY): FKM exposes input-boost +
  stune. Begonia has touch boost + schedtune — unify as one input path (touch
  IRQ → boost, with decay) instead of two knobs.

### 11.2 From Linux 6.x mainline (adapt, don't rebase)
- 11.2.1 EEVDF lesson (EASY as tuning): EEVDF (6.6+) prioritizes
  latency-sensitive short slices via earliest-virtual-deadline + decaying lag
  so sleepers can't game the system. On 4.14 CFS, emulate with short-slice
  interactive bias (low `sched_latency`/`min_granularity` already 4ms/500us) +
  prefer-idle for top-app, keeping anti-gaming decay.
- 11.2.2 DAMON-style proactive reclaim (MODERATE): DAMON monitors access
  patterns for proactive reclaim + LRU sorting. Add lightweight idle-page
  tracking preset (cold-page first reclaim) rather than full DAMON backport.
- 11.2.3 sched_ext lesson (SPEC only): 6.12+ allows BPF schedulers with safe
  fallback to fair-class. Takeaway for begonia: keep ONE arbiter
  (gaming_mode) with safe fallback to balanced on error — same pattern, no BPF.
- 11.2.4 Util-clamp + capacity-aware placement (EASY): mainline EAS/uclamp is
  the mature form of the existing capacity_margin + boost tables — formalize
  per-task uclamp min/max presets (RenderThread min, background max).

### 11.3 From ROG Phone / OnePlus gaming phones
- 11.3.1 X Mode background freeze (EASY/MODERATE): ROG X Mode kills
  RAM/battery-sapping app activity from a customizable list. Implement as
  gaming cgroup freeze + background-app list, not process killing.
- 11.3.2 AirTrigger-style extra inputs (EASY): ROG ultrasonic edge/rear zones
  give "two extra fingers". No such hardware on begonia — emulate with
  volume-key / edge-swipe remap profiles during gaming mode only.
- 11.3.3 Touch latency path (EASY, extends 4.4): ROG 300Hz sampling / 24.3ms
  latency; OnePlus offloads touch to a dedicated chip (330Hz + 3200Hz instant).
  Begonia analog: touch IRQ affinity to a pinned little core + RT priority for
  the touch thread + report-rate preset, all under gaming_mode with decay.
- 11.3.4 Bypass + steady + scheduled charging (EASY, extends 7.x): ROG
  dual-cell + steady/scheduled charging + charge上限. Begonia has bypass —
  add steady-current gaming preset + overnight schedule (finish by wake time).
- 11.3.5 Side-port / cooler lesson (INFO): ROG solves cable-in-landscape with
  side ports + AeroActive cooler. No kernel action — just don't fight cooler
  accessories with aggressive thermal throttling when USB-OTG cooler present.
- 11.3.6 Frame-interpolation caution (DO NOT ADD): OnePlus HyperRendering does
  driver-level interpolation. Out of scope for Mali-G76 on 4.14 — stick to
  frame-pacing + vsync-deadline rescue already present.

### 11.4 From Pixel + Samsung battery stacks
- 11.4.1 Samsung Basic/Adaptive/Maximum (EASY): Basic = 100%→pause→resume at
  95% hysteresis; Adaptive = Maximum overnight, Basic before wake (learned
  sleep); Maximum = 80% cap. Begonia has limit 80 + guard — add hysteresis +
  sleep-aware overnight mode.
- 11.4.2 Pixel Adaptive Charging + Limit-to-80 (EASY): pause at 80%, finish to
  100% an hour before unplug; shield UI state. Same overnight scheduler as
  11.4.1, shared implementation.
- 11.4.3 Pixel Battery Health Assistance (MODERATE): staged max-voltage derate
  from ~200→1000 cycles + charge-speed retune + capacity % readout
  (Normal/Reduced). Add cycle counter + staged voltage/current derate table +
  read-only capacity estimate; never silently cut runtime — expose counters.

### 11.5 From iOS (memory / display / haptics)
- 11.5.1 Jetsam bands + HWM + compressor-space (MODERATE): iOS has no swap —
  compressed pages + 21 priority bands + per-process high-water-mark kills;
  `compressor-space` shortage itself is a kill reason. Begonia analog: LMK
  priority bands (foreground last) + HWM cap for runaway WebView/camera +
  zram-fullness as an explicit reclaim/kill signal instead of fixed minfree.
- 11.5.2 ProMotion range API (MODERATE, panel-dependent): iOS varies 10-120Hz
  with per-app preferred frame-rate ranges (games get 30/60 priority). Add
  display-QoS *range* hint (idle 30 / video 24-30 / game 60) instead of fixed
  peak clocks; no-op if the begonia panel can't vary refresh.
- 11.5.3 Core Haptics envelopes (EASY/MODERATE): iOS designs patterns with
  ~5ms precision on a large Taptic Engine; Android timing is coarse and begonia
  actuator is small. Add short waveform-composition presets with strict duration
  caps rather than raw 3.3V strength (extends 6.2 safely).
- 11.5.4 Timer coalescing / App Nap (EASY): iOS groups background timers.
  Powersave preset: timer slack + batched wakeups + deferred sync (with the
  fsync lifetime guard from 3.2).

## 12. Updated build order (with borrowed items merged)
1. Memory: MGLRU/PSI evaluation + zram/reclaim presets + Jetsam-band LMK
   (2.x + 11.1.2 + 11.5.1) with scheduler presets (1.x + 11.2.1/11.2.4).
2. Gaming arbiter: X-Mode freeze + touch path + FPSGO tables
   (11.3.1-11.3.3 + 4.x) + per-app profiles hook (11.1.6).
3. Battery: Samsung/Pixel hysteresis + overnight + cycle derate
   (7.x + 11.3.4 + 11.4.x).
4. Display/sound/haptics: warm preset + HBM auto + envelope library
   (11.1.8-11.1.9 + 11.5.2-11.5.3) + GPU idler (11.1.10).
5. I/O + net presets: deadline tuning + BBRv3 + WiFi CAM formalized
   (3.x + 8.x + 11.1.1/11.1.3).
6. Hardening track in parallel (spec only): CFI/ThinLTO/SCS evaluation
   (11.1.4); never at the cost of stability.
