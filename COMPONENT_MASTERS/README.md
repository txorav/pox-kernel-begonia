# Component masters — index (research only, no code changed)

Per-component deep research, one file each. All proposals, nothing implemented.

| # | Component | File |
|---|---|---|
| 01 | CPU + scheduler (A76/A55, CFS/EAS/schedtune/FPSGO/PPM) | 01-cpu-scheduler.md |
| 02 | GPU + display (Mali-G76, GED, color, HBM, 60Hz panel) | 02-gpu-display.md |
| 03 | Memory (reclaim, ZRAM, KSM, LMK/PSI, MGLRU/DAMON) | 03-memory.md |
| 04 | Storage / I/O (schedulers, UFS, fsync, dirty, F2FS) | 04-storage-io.md |
| 05 | Camera + media (GW1, ISP QoS, vcodec, color, launch boost) | 05-camera-media.md |
| 06 | Touch + input (Novatek, DT2W, gestures, input boost) | 06-touch-input.md |
| 07 | Audio + haptics + LEDs (mt6359, vibrator, HBM, torch) | 07-audio-haptics-leds.md |
| 08 | Battery + charging + thermal (JEITA, bypass, CLATM/PPM) | 08-battery-charging-thermal.md |
| 09 | Connectivity (WLAN gen4m, BT, TCP/BBR, netfilter) | 09-connectivity.md |
| 10 | System misc (wakelocks, SCP/sensors, modem, suspend, observability) | 10-system-misc.md |

Cross-cutting docs: `../SECURITY_KNOWN_ISSUES.md` (binding constraints on new
nodes: 0644 + CAP_SYS_ADMIN, read-only 0444), `../FUNCTIONALITY_ROADMAP.md`
(sections 1-12 synthesis + build order).
