# 06 Touch + Input — component master (research only, no code changed)

Panels: NT36672A active (Novatek), FTS/GT variants in tree. DT2W via double_click bridge.

## Existing (file:line)
- xiaomi_touch framework: 20 modes (`xiaomi_touch.h:75-97`), ioctl dispatch
  (`xiaomi_touch.c:62-105`), Pox shims game_mode/sensitivity
  (`:392-438`, sysfs 0664 `:440-482`).
- NT36672A: gesture IDs C/W/V/DBL/Z/M/O + slides 21-24 (`nt36xxx.c:963-986`),
  wakeup report (`:994-1073`), threaded IRQ FIFO-98 + nice -20 (`:1403-1408`),
  suspend 0x13 gesture vs 0x11 deep-sleep (`:2807-2890`), resume reflashes FW
  every time (`:2899-2951`). No IRQ affinity set. Report_Rate/Doubletap/Grip
  slots defined but unmapped in NVT (`:1795-1860` vs set_cur `:1913-2019`).
- DT2W: `double_click.c:7-60` static flag + sysfs 0664; consumed by NVT/FTS/panels.
  FTS has glove/cover/charger modes (`focaltech_ex_mode.c`); NVT has no glove.
- Input boost: tchbst `ktch.c:66-135` (1.50/1.53GHz floors + 60% TA uclamp, 80ms
  hold, FIFO-90) + utch userspace path; gaming_mode couples game/sensitivity +
  schedtune boost + 500us ramp.

## Borrowed ideas
1. Touch IRQ affinity (pin to little core) + keep RT thread — HIGH, -0.5-2ms jitter.
2. Report-rate presets via Touch_Report_Rate=9 (needs Novatek FW cmd) — MED.
3. Palm/glove parity (port FTS C0/C1/8B pattern to NVT tolerance preset) — HIGH.
4. Sweep-to-sleep + pocket guard (reuse decoded slides 21-24, gate on
   p_sensor/palm; ElementalX parity) — HIGH.
5. AirTrigger emulation via edge rectangles → KEY_GAMING_LEFT/RIGHT, only when
   Game_Mode=1 (no ultrasonic HW; software only) — MED, needs per-game calibration.
6. DT2W hardening: single source dt2w↔db_wakeup↔panel gating + debounce — HIGH.
7. Game-aware touch boost (all MT slots, 80ms hold adapt when game=1) — HIGH.
8. Game preset bundle fix (sensitivity compose not overwrite, orientation-aware
   edge) — HIGH.
9. Resume-latency cut (skip FW reflash when version matches) — MED.
10. Touch latency telemetry (IRQ→sync ns, SPI fails, ESD) in ext_proc — HIGH.

Order: 1→6→7→8→4→5→3→10→2→9.
