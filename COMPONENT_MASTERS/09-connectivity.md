# 09 Connectivity — component master (research only, no code changed)

MTK full-MAC WLAN gen4m + WMT/BT + BBRv1 + netfilter. No init.gaming.rc net hooks
in tree (gaming_mode has ZERO WLAN/BT/TCP hooks; `:388` bbr line is display-only).

## Existing (file:line)
- WLAN power: PARAM_POWER_MODE CAM/MAX_PSP/Fast_PSP (`wlan_oid.h:591-596`),
  default FAST_SWITCH (`config.h:548`), U-APSD off by default (`:553`, SUPPORT=1
  `:856`); choke point `nicConfigPowerSaveProfile()` (`nic.c:2056-2124`);
  cfg80211/wext set_power_mgmt (`gl_cfg80211.c:2050-2068`); no-TIM→CAM
  (`scan.c:2722`); P2P paths; WoWLAN priv cmds (`gl_wext_priv.c:9420-9766`).
- Roam/PNO: discover timeout 10s (`roaming_fsm.h:81`), PNO bucket 30s
  (`wlan_lib.h:356`).
- TCP: BBRv1 only (`tcp_bbr.c:79-177`); defconfigs CUBIC..BBR + DEFAULT_BBR.
  DIVERGENCE: user defconfig FQ/FQ_CODEL NOT SET (`:1064-1065`) vs apatch =y —
  fallback pfifo_fast; fix to fq. FASTOPEN not enabled (user). RPS/RFS/XPS =y.
- BT: SCO/eSCO tables (`hci_conn.c:38-60`); MTK_COMBO_BT=y; WMT func on/off
  (`stp_chrdev_bt.c:613`).
- Wakelock blocker covers wlan_wake/wow/extscan/rx, pno, wmt, netmgr
  (`wakeup.c:590-598`), exposed via perfmgr proc.

## Borrowed ideas
1. Gaming CAM preset (call nicConfigPowerSaveProfile(CAM) on game>0, restore
   Fast_PSP on exit) — easy, 5-30ms tail cut.
2. Per-BSS pin, no roam-hop in game (raise discover timeout/PNO period; RSSI
   floor escape; ROG-disconnect lesson) — easy.
3. Scan backoff in doze (longer PNO/extscan in powersave -1) — easy.
4. Enable fq qdisc (fix user/apatch skew; default_qdisc=fq via init) — trivial;
   BBR actually paces, bufferbloat down.
5. Keep BBRv1; DEFER BBRv3 backport (thousands of lines into 4.x TCP) — decision.
6. RX steering pin (RPS cpus to A55s during game; script only) — easy.
7. BT SCO low-jitter preset for game+voice (low max_latency entries, bounded
   retrans; prefer 5GHz in game for 2.4GHz contention) — MEDIUM.
8. FW coex bias to WiFi in game (existing CMD path; FW opaque, A/B test) — MEDIUM.
9. Wakelock-blocker gaming exception: UNBLOCK wlan_rx_wake in game (keep
   extscan/pno blocked) — easy, prevents RX-suspend races.
10. U-APSD VO/VI-only in game (default bitmap 0; fallback CAM on bad APs) — MEDIUM.

Order: 4→1→2→9→6→3→10→7→8; skip 5.
