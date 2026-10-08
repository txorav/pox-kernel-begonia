# 07 Audio + Haptics + LEDs — component master (research only, no code changed)

Codec MT6359 + AW Smart-K speaker PAs (AW87359/AW87519) + LM36273 backlight + MT6360 flashlight.

## Existing (file:line)
- mt6359 POX gains: headphone 0..8dB (`mt6359.c:6895-6942`, ZCD_CON2, via ZCD
  ramp `:661-695`), mic 0..7 (`:6944-6990`, MICAMP1/2/3; note 5..7 out-of-enum,
  reserved/aliased). Sysfs 0664 + sound_control kobj (`:7025-7033`); proc mirrors
  0644 CAP-gated in gaming_mode. Native TLVs: HP -22dB+1dB, UL 0-24dB.
  No POX hook for speaker path (external AW PAs, DTS `mt6853.dts:6177`).
- Vibrator: PMIC table 1.2-3.3V sparse (`mt6359p-regulator.c:304`); DTS default
  idx9=2.8V; cust `vib_vol_max=0x0D` unlock (`vibrator.c:102`); POX strength
  0..0x0D (`:133-155`). Durations rounded <9ms→25ms, cap 15s
  (`vibrator_drv.c:89-96`); OC IRQ stop (`:106-110`); vmax sysfs 0644.
  No thermal foldback, no waveform sequencer.
- HBM: L1 22mA / L2 25.3 / L3 27.5 (`leds-lm36273.h:22-27`),
  `pox_lm36273_hbm_set(0-3)` (`leds-lm36273.c:135-181`); manual only, deferred
  write when display off (`:147`), no lux/timeout/thermal.
- Torch: 23 levels 25..750mA (`flashlights-mt6360-mt6785.c:107-122`), HW WDT
  1248ms + strobe 400ms + hrtimer auto-off (`:560-577`), charger-voltage
  arbitration (`:375-409`). POX grading 0..10 perceptual + direct (`:788-821`),
  clamp 24=325mA/ch; POX path timeout_ms=0 (NO software timeout), LED classdevs
  torch-light0/1/2+flashlight. Gaps: no ON-time limit, no thermal step-down,
  no camera mutual exclusion.

## Borrowed ideas
1. Per-device audio preset tables in userspace over existing gains — EASY.
2. Speaker DRC/protection policy for AW PAs (HAL/profile) — MODERATE.
3. Persistent safe-volume ceiling (+4dB default, opt-in to +8, survives reboot;
   Samsung lesson: explicit opt-out) — EASY.
4. Haptic waveform library (short envelopes, per-effect caps ≤200/500ms) instead
   of raw voltage (iOS Core Haptics lesson; ERM smears ~5ms) — EASY/MOD.
5. Sustained-voltage ceiling: 3.3V bursts ≤50ms only, fold to ≤2.8V sustained —
   EASY.
6. HBM auto (lux steps L0-L3, hysteresis + daylight timeout + thermal veto) —
   EASY.
7. Torch software timeout 3-5min + 30s warn (POX path timeout=0 today) — EASY.
8. Torch thermal step-down 24→15→9→off with hysteresis — MODERATE.
9. Torch camera arbitration (yield on FLASHLIGHT_SCENARIO_CAMERA, restore after;
   surface in torch_info) — EASY/MOD.
10. Read-only audio_haptics_status meta-node 0444 (HP/mic/vib/HBM/torch) — EASY.

Non-goals: kernel DTS/7.1 virtualizer (userspace like ROG/Dirac), sustained 3.3V,
silent safe-volume removal, new 0666 nodes.
