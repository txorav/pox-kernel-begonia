# 08 Battery + Charging + Thermal — component master (research only, no code changed)

Charger mtk_charger (begonia) + dual-switch + JEITA + CLATM/PPM + battery_guard.rc.

## Existing (file:line)
- `mtk_charger.c:1805-1809`: bypass_mode, charge_limit=100, thermal_guard,
  temp_limit=46.0C, bypass_reason 0-3. Decision `charger_check_status:1811-1925`:
  manual bypass wins (reason 1), cap reached (reason 2) + 3% hysteresis hold,
  thermal guard (reason 3) + 2.0C release hysteresis. JEITA machine `:1096-1214`
  (bands 2.20/1.80/1.32/0.44A, CVs 4.24/4.34/4.24/4.04, normal 4.35V).
- Sysfs `/sys/kernel/battery_protection/`: bypass/charge_limit(50-100)/
  thermal_guard/temp_limit(30-55C) 0664, status/soc/temp 0444 (`:3416-3578`).
- Fast-charge flag default 1 (`:3523-3537`, proc only); consumed in
  `mtk_dual_switch_charging.c:336-357` (1.50A USB-std / 2.00A non-std when ON,
  else 0.50A DT). Standard/PE/PD tables + chg1/chg2 split, PE stop 85 / PD 80.
- Perfmgr bridge: battery_bypass/limit/status/fast_charge proc 0644 CAP-gated.
- Ramdisk `init.battery_guard.rc` generated (`build.sh:590-606`): limit 80,
  guard 1, temp 39.
- Thermal: CLATM CFG0/1/2 (Tj 75/78/75, CPU floors 500/1200/600mW,
  `clatm_initcfg.h:20-51`); cooler_fps 60fps floor + gaming override
  (`mtk_cooler_fps.c:282-322`); BCCT current cooler exists; PPM screen-off
  Little 500MHz floor + Big 1.5GHz cap (`mtk_ppm_policy_lcm_off.c:49-78`).

## Borrowed ideas
1. Cap hysteresis 3%→5% + expose hold state (Samsung Basic parity) — trivial.
2. Thermal-guard entry debounce 2/3 polls (NTC noise) — easy.
3. Overnight adaptive: hold 80%, finish by alarm (Samsung Adaptive / Pixel /
   iOS parity) — MEDIUM (alarm + UX); missed-alarm risk.
4. Cycle-counted CV derate 200→1000 cycles (Pixel Health Assistance; hooks at
   `mtk_charger_init.h:18`, `mtk_dual_switch_charging.c:648`) — MEDIUM.
5. Capacity/cycle readout next to soc/temp — easy if gauge exposes.
6. Steady-charge W preset via input_current_limit clamp (ROG parity) — easy.
7. Auto-bypass on gaming+plugged (mirror cooler_fps gaming read; show AUTO vs
   MANUAL; yield to manual off + JEITA) — easy, medium-high gamer gain.
8. Skin-temp feedforward into charge current (bridge BCCT + TPCB; single
   actuator to avoid double-throttle with JEITA) — MEDIUM.
9. Screen-off charge preset (skip PE20 bump when ppm_lcmoff active) — MEDIUM.
10. Monthly 100% calibration pass when capped <100 (iOS/Samsung) — EASY-MED.

Order: 1,2,6,7 → 3,4,5 → 8,9,10.
