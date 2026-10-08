# 05 Camera + Media — component master (research only, no code changed)

Sensors: Samsung GW1 (64MP full @15fps, 16MP binned @30, 1080p slow-mo modes),
plus IMX519/HM2 reference tables. ISP: peak-primed 560MHz (Pox floor).

## Existing (file:line)
- imgsensor core: `imgsensor.c:403-437` close/power seq (launch hook),
  `:442-479` check_is_alive powers ON for ID (cold-launch cost).
- GW1 `s5kgw1spmipiraw_Sensor.c:71-174`: pre 16MP@30, cap 64MP@15, cap1/2 4MP@120,
  normal_video 16MP@30, hs 1080p@240, custom1 1080p@120, slim VT low-power;
  `:411-439` fps math; huge I2C init tables (launch latency).
- camisp `cam_qos.c:560-587`: primed 560MHz + Pox floor (clamp <560→560).
  Siblings (mt6885/mt6873) lack the clamp — divergence risk.
- vcodec: enc `mtk_vcodec_enc_pm.c` (VENC_FREQ + 9 SMI BW reqs, peak-hold);
  dec `mtk_vcodec_dec_pm.c:315-349` Pox warm floor (peak/idx1/idle instead of
  collapse to min). LAT arch `MTK_VDEC_LAT=1` present.
- Color: `ddp_color.c:126-192` modes 0-3 + Cam flat base; S-Log3 zeroes SHP,
  flattens contrast (full PQ path, no cheap preview variant).
- gaming_mode: camera_4k60 flag (no driver enforcement yet), slog3→color mode,
  launch boost (DRAM + schedtune 40 + prefer_idle, refcounted, 750ms fixed decay).

## Borrowed ideas
1. Balanced (~416MHz preview/VT) vs Peak (560MHz 4K60/burst) ISP floors by
   scenario hint (Pixel StreamUseCase lesson) — HIGH feasibility (1 file + flag).
2. Per-scenario fps caps (VT/slim→30, preview→30, video→60 only if 4K60 force)
   instead of global force; kills preview+record double-work heat — HIGH, test
   Camera2 FPS ranges.
3. Thermal-aware launch decay (750ms cool / 350ms warm / 0 on SEVERE) — HIGH.
4. Decode low-latency whitelist: plumb MTK `vdec-lowlatency=1` +
   `PARAMETER_KEY_LOW_LATENCY` to pmqos prelock/begin + BW hold for
   Meet/WhatsApp/IMS ≤1080p foreground — MED, high call-latency payoff.
5. Zero-shutter pre-roll (iPhone-style): keep GW1 `pre` streaming ring during
   camera foreground; shutter = pick-back, skip warm ID check, defer cap switch —
   MED orchestration, high lag win; auto-stop on bg/thermal.
6. Deferred finalize (fast flat/S-Log3 proxy now, HQ fusion idle) — MED-HIGH
   (HAL work), high burst win.
7. Burst auto-degrade (64MP→bin path + restore color after) — HIGH kernel assist.
8. Encode fast-lane for RTC (constrained-baseline-ish, short GOP, whitelisted
   RTC only) — MED.
9. S-Log3 cheap preview variant (gamma-only, skip 3D-LUT/CCORR; full grade on
   finalize) — HIGH.
10. QoS governor audit: hold CPU+DRAM+ISP jointly 750ms then decay TOGETHER
    (FUSE-paper anti-spiral) — HIGH read-only first.

Order: 1→3→2 (cool sustained first), then 4+8 (calls), 5+6+7 (shutter/burst),
9+10 polish. Kirisakura lesson holds: schedule+IRQ+DVFS coordination beats raw
frequency pinning.
