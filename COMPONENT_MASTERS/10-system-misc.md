# 10 System: wakelocks / sensors / modem / suspend / observability — master (research only)

## Existing (file:line)
- Wakeup blocker `wakeup.c:570-613`: boolean default ON, 9 hardcoded names
  (wlan_wake/wow/extscan/rx, sensor_ind, ccci_fsm, netmgr/pno/wmt), silent drop
  (event_count++ but no active/combined++). Proc boolean 0644 CAP-gated
  (`gaming_mode.c:1285-1328`); chmod in `build.sh:430-441`.
- SCP: `scp_awake_lock()` IPI + busy-wait + WARN_ON/reset on timeout
  (`scp_awake.c:58-133`); WDT/IRQ dispatch (`scp_irq.c:33-102`); DVFS holds
  pm_stay_awake. SensorHub sources (SCP_nanoHub ws/sync_time, situation locks,
  alsps ps_wake_lock) mostly NOT in blocker list (only sensor_ind).
- Modem eCCCI: `ccci_fsm` wakelock (`ccci_fsm.c:825`) = coarse block-all; per-MD
  peer_wake (`modem_sys1.c:306-308` HZ window) + per-port RX wakelocks (HZ/2)
  NOT blocked — data path still wakes AP. Suspend coordination via
  AP2MD_LOWPWR + MD wakeup-src dump.
- Suspend: `kernel/power/suspend.c` + MTK LPM hooks (`mtk_lpm_module.c:256,308`);
  FREEZER + SUSPEND_FREEZER + CGROUP_FREEZER all y. AUTOSLEEP NOT SET
  (`defconfig:628`) → no /sys/power/autosleep; Doze is framework-driven.
- Observability free: per-source sysfs RO (`wakeup_stats.c`), debugfs table
  (`wakeup.c:1119-1247`), resume-cause RO (`wakeup_reason.c:298-369`), abort log
  (`wakeup.c:956-980`).
- DT2W `double_click.c:48` sysfs 0664 with NO capable() check — flag to tighten
  to 0644 + CAP_SYS_ADMIN (SECURITY drive-by).
- No per-app/package hook in performance/ (correct — kernel must not parse
  package names; numeric profile IDs only).

## Borrowed ideas
1. Allowlist blocker + per-mode sets (0 off / 1 balanced current 9 / 2 deep +
   sync_time/ps batch / 3 extreme) + compat boolean + new block_mode 0644 —
   HIGH, deep-idle aborts down; exempt MD_WDT/CCIF voice paths.
2. Blocked-event counters (atomic64 per-source + total; RO sysfs pattern) —
   HIGH additive, proves policy (FKM-health style).
3. Read-only powersave_profile meta-node 0444 (mode + sets + blocked_total +
   suspend success/abort + last_resume_reason) — HIGH.
4. Cgroup freeze list, X-Mode analogue (userspace owns policy: freeze bg apps/
   cgroups minus allowlist; kernel thin hint node numeric UIDs) — MEDIUM;
   foreground exemption mandatory (ANR risk).
5. Doze/deep-idle preset without autosleep Kconfig (prefer s2idle, block
   scan/sensor sets, shorten peer_wake HZ→HZ/2 under deep + counters) — MEDIUM.
6. Pocket-mode DT2W auto-disable (prox/palm gate + debounce; fix 0664→0644 +
   capable) — HIGH.
7. Quiet-boot/loglevel preset via existing printk knobs (restore verbose on
   exit; never gate oops/panic) — HIGH.
8. Modem wake shredding guard (keep FSM blocked in deep but NEVER MD_WDT IRQ
   wake nor D2H_EXCEPTION_INIT; rate-limited peer-wake notice) — MEDIUM.
9. SCP/sensor batch preset (raise batch timeouts; read-only awake stat; never
   lengthen SCP_AWAKE_TIMEOUT; block sensor_ind only when batched) — MEDIUM.
10. Suspend-abort feedback loop (userspace steps extreme→deep→balanced on abort
    rate with hysteresis) — HIGH.

NOT recommended: enabling AUTOSLEEP (global PM semantic change), kernel package
parsing (layering/privacy), new 0666/0664 nodes, touching selinux hooks or
fs/proc/inode.c (A1 regression). Binding per SECURITY_KNOWN_ISSUES A1-A2:
writable = 0644 + capable(CAP_SYS_ADMIN) + build.sh chmod; read-only = 0444.
