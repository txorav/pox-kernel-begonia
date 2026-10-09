# Security audit — safe-rootless by design (all features kept)

Base: Linux 4.14.357, arm64, MT6785 (begonia). Branch: `onyx`.
Status: **ALL FEATURES PRESERVED. Hardened where it can't brick/corrupt.**

Pox stands out by keeping what others remove: graded rootless torch,
18W safe fast-charge, universal USB OTG, True Tone, 4K60, gaming touch.
Safety lives in hardware guards, not feature removal.

---

## A. Safe-rootless torch grading — KEPT + HARDENED (standout)

- **Design (intentional):**
  - `drivers/misc/mediatek/performance/gaming_mode.c:646,1010`:
    `torch_brightness` / `flashlight_brightness` proc+sysfs stay `0666`
    with **no** `CAP_SYS_ADMIN` so stock flashlight apps work without root.
  - `drivers/misc/mediatek/flashlight/flashlights-mt6360-mt6785.c:838`:
    `torchbrightness` sysfs stays `0666` for the same reason.
  - `security/selinux/hooks.c:1723`: `is_rootless_allowed_node()` narrowly
    confines the LSM exception to 4 names only
    (`torchbrightness`, `torch_brightness`, `flashlight_brightness`,
    `torch_info`), sysfs/proc only, world-readable only, no
    `d_find_any_alias` (no dcache contention on 8 cores). Now also
    handles `LSM_AUDIT_DATA_FILE` so `open()` works consistently.
  - `fs/proc/inode.c:450`: `S_PRIVATE` on those 3 proc aliases only, so
    ROMs without custom SELinux policy still get working torch.
- **Guards (why it can't fuck the device):**
  - `pox_torch_val_to_sel()`: clamp sel `0..24` (25..325mA/ch, 650mA dual max).
    `0..10` grading table, `11..255` scaled, `>255` clamped.
  - `pox_torch_brightness_set()` + both proc/sysfs stores: `0..255` clamp,
    `20ms` anti-strobe (`-EBUSY` on flood), mutex, 5-min auto-off
    `delayed_work`, `-ENODEV` if devices not ready.
  - Oversize writes (`>=16B`) rejected with `-EINVAL` (no truncation misparse).
- **Everything else locked down:**
  - All other 17 `/proc/perfmgr/*` + 6 sysfs nodes: `0644`/`0444` +
    `capable(CAP_SYS_ADMIN)` — gaming, color, HBM, battery, touch,
    audio, vibrator, wakelock, fast_charge, dt2w, mic, fsync, camera.
  - `camera_4k60` proc fixed `0666` → `0644` (handler already required
    `CAP_SYS_ADMIN`; world-writable was redundant exposure).
  - `build.sh` (`init.gaming.rc`): torch `0666` documented as by-design,
    `camera_4k60` now `0644`, USB dir fixed `0755` (see B).

### A2. Dynamic fsync — KEPT + GATED
- `fs/sync.c:45`: `0644` + `CAP_SYS_ADMIN`. Auto-disarm on powersave/
  balanced exit in `gaming_mode.c:287,293`. No silent durability loss
  for normal apps.

### A3. Charger bypass + 18W/6W fast-charge — KEPT + SAFE FLOOR
- `battery_bypass/limit/fast_charge`: `0644` + `CAP_SYS_ADMIN`. No world writes.
- 18W dual (4.2A) only on `STANDARD/NONSTANDARD/APPLE_2_1A` when
  `battery_temp < 48C`; 6W PC-USB (1.2A in) only on host types.
  Thermal floor (`2.0A` AC / `1.5A` USB) applies **only** `<48C` with
  `pr_info_ratelimited`; at `>=48C` JEITA + thermal daemon win and can
  pull to 0. Hysteresis 5% on charge-limit cap. `fast_charge=1` default
  kept — still opt-out via `/proc/perfmgr/fast_charge 0`.

---

## B. USB OTG — ALL DRIVERS KEPT, perms fixed so USB keeps working

- `CONFIG_USB_ACM/PRINTER/WDM/SERIAL_{GENERIC,CH341,CP210X,FTDI,PL2303}/
  USBNET_{AX8817X,CDCETHER,NCM,RNDIS}/IPHETH` stay `=y` (DACs,
  controllers, serial, ethernet, iPhone tethering).
  `USB_SERIAL_CONSOLE` kept (no feature removed).
  `USB_ANNOUNCE_NEW_DEVICES` kept for OTG plug-and-play logging.
- `build.sh`: `chmod 0666 /dev/bus/usb` on a **directory** stripped `+x`
  and broke enumeration. Fixed to `0755` dir + `0666` on
  `/dev/bus/usb/*/*` + `ttyUSB0-3/ttyACM0-1` — every adapter still
  plug-and-play, enumeration never breaks. No device corruption path.

## C. Stability guards — nothing that reboots/corrupts

- Watchdog `kwdt_thread`: kept precise `usleep_range()` (reverted
  `schedule_timeout_interruptible(jiffies)` experiment — coarse jiffies
  + interruptible kick risked spurious reset under load).
- `ktch` boost `wait_event_interruptible` + `ion_history`
  `msleep_interruptible` kept (clean `kthread_should_stop` /
  `fatal_signal_pending` handling, no missed-kick path).
- Touch `SCHED_FIFO 98/90 + nice -20` kept for zero frame-drop feel;
  watchdog precision above guarantees kick even under touch flood.
- `child_runs_first=0`, `MD1_SUPPORT=12`/`MD_GENERATION=6293`,
  `nomerges=0`, `read_ahead 128KB`, VM watermarks stock — all kept from
  `e7d9a65d0` freeze/modem/VFS fix. No aggressive reclaim that OOMs.
- ISP dynamic DFS kept (no forced `560MHz` floor); `camera_4k60_force=0`
  default so 4K60 heat is opt-in per recording, not always-on.

## D. Hardening deltas (`arch/arm64/configs/`) — kept for root compat

| Setting | `begonia_user` | `begonia_apatch` | Note |
|---|---|---|---|
| `CONFIG_KALLSYMS_ALL` | y | y | APatch/root compat (KASLR bypass aid — accepted tradeoff) |
| `CONFIG_RANDOMIZE_BASE` (KASLR) | y | y | Enabled everywhere |
| `CONFIG_FORTIFY_SOURCE` | y | y | On |
| `CONFIG_SLAB_FREELIST_RANDOM/HARDENED` | y | y | On |
| `CONFIG_INIT_STACK_ALL_ZERO` + `CONFIG_INIT_ON_ALLOC_DEFAULT_ON` | y | y | On |
| `CONFIG_REFCOUNT_FULL` | y | y | On |
| `CONFIG_SECURITY_YAMA` | y | y | On |
| `CONFIG_BPF_UNPRIV_DEFAULT_OFF` | y | y | Unpriv BPF off |
| `CONFIG_USERFAULTFD` | y | y | Kept (root-tool compat; race-aid tradeoff documented) |
| `CONFIG_KPROBES` | not set | not set | Off everywhere |
| `CONFIG_MODULE_SIG` | not set | not set | Off for root modules (tradeoff documented) |

---

## E. Upstream CVE status (honest)

- **x_tables CVE-2021-22555:** compat pad-zero in `net/netfilter/x_tables.c`
  (match/target). Partial — full `xt_alloc_table_info` audit still open.
- **rawmidi CVE-2020-27786:** `buffer_ref EBUSY` guard in `sound/core/rawmidi.c`. Done.
- **overlayfs CVE-2023-0386:** `CAP_FOWNER` SUID strip in `fs/overlayfs/copy_up.c`. Done.
- **DirtyPipe CVE-2022-0847 / Binder CVE-2019-2215 / CMDQ CVE-2020-0069:**
  claimed before, **UNVERIFIED** in this tree (`fs/pipe.c` still classic
  `anon_pipe_buf_ops.can_merge=1`, no pipe commit in log). Do not claim
  patched until backport + PoC test lands. No feature depends on them.

---

## F. Full deep audit 2026-10-09 — branch `onyx-resukisu` (ReSukiSu v4.2.0 + SuSFS v2.3.0 + NoMount 2.0.0)

Scope: `drivers/misc/mediatek/performance/gaming_mode.c` (23x `/proc/perfmgr/*` + 9x `/sys/kernel/*`), `security/selinux/hooks.c:1723-1780`, `fs/proc/inode.c:450-460`, `drivers/misc/mediatek/flashlight/flashlights-mt6360-mt6785.c`, `drivers/power/supply/mediatek/charger_begonia/`, `drivers/misc/mediatek/performance/tchbst/kernel/ktch.c`, `drivers/input/touchscreen/xiaomi/xiaomi_touch.c`, `mm/memcontrol.c` xswapd, `fs/sync.c`, `drivers/scsi/ufs/`, `drivers/kernelsu/*` + `fs/susfs.c` + `fs/nomount/*`, `Makefile:2-5` 4.14.357 EOL, `build.sh` ramdisk, `verity_dev_keys.x509`.

### F0. P0-Critical (fix first, no feature removal)

- **Z1 LSM basename bypass + `S_PRIVATE` audit kill — `security/selinux/hooks.c:1723-1780,1795-1799,3261-3265` + `fs/proc/inode.c:450-460`:**
  `is_rootless_allowed_node()` matches `d_name` only (`torchbrightness|torch_brightness|flashlight_brightness|torch_info`), no parent/device/UID check; `S_PRIVATE` + early `return 0` skips `avc_has_perm` + audit in both `inode_has_perm` and `inode_permission`. `torchbrightness` allowed in LSM but missing from `S_PRIVATE` set — inconsistent. RCU fallback `hooks.c:1758-1768` uses `name` after `rcu_read_unlock()`. `ioctl_has_perm:3757-3773` + `binder_transfer:2231` don't call bypass — inconsistent mediation. Forensically invisible (no AVC denial). Any future node with same basename inherits bypass globally.
- **Z2 Torch world-write strobe/drain/DoS — `gaming_mode.c:2056-2060,1320-1330` + `flashlights-mt6360-mt6785.c:839-933`:**
  `0666`, no `capable()`. Clamp `0..255 -> sel 0..24` (650mA dual) + 20ms `last_jiffies:860-863,957-959` check-then-set with no lock (N-thread bypass) + 5-min `delayed_work:908-909` resettable by any `>0` write (infinite ~2.4W drain). `value==0` skips throttle -> `ON-OFF-ON` ~23Hz inside 3-30Hz photosensitive band. `store:930-932` ignores `-EBUSY/-ENODEV`, returns `size` (lie). Spinning writer starves camera (`-EBUSY` DoS).
- **Z3 `ktch` `0666` no-CAP — `drivers/misc/mediatek/performance/tchbst/kernel/ktch.c:414-430`:**
  `tb_enable/tb_core/tb_freq/tb_clstr` `0666`, handlers `:152-275` have no `capable()` check. Any `untrusted_app` toggles boost policy.
- **Z4 DirtyPipe vulnerable — `fs/pipe.c:249-250`:**
  `anon_pipe_buf_ops.can_merge=1`, `anon_pipe_buf_nomerge_ops:257` unused on write path. Upstream fix not backported. Local privesc.

### F1. P1-High (privilege boundary missing, DAC-only today)

- **Z5 Charger sysfs no-CAP — `drivers/power/supply/mediatek/charger_begonia/mtk_charger.c:3607-3614,3446-3516,3588-3593`:**
  `bypass_mode/charge_limit/thermal_guard/temp_limit/fast_charge` `0664`, no `capable()`. Proc mirrors `gaming_mode.c:1400-1749` have `CAP_SYS_ADMIN`. Same for legacy `sw_jeita/pe20/input/chg1/chg2/BatteryNotify:1251-3294` `0664` no-CAP. Today `root:root` so DAC blocks unpriv, but any group grant / `chmod` regression = thermal/battery abuse. 18W 4.2A `mtk_dual_switch_charging.c:335-504` gated on `temp<48C` only; floor `:519-595` lifts daemon to 2.0A/1.5A while `<48C`. JEITA-stop `:1823-1828` cannot be overridden (good); T3-T4 45-60C still charges.
- **Z6 Touch `0664` no-CAP — `drivers/input/touchscreen/xiaomi/xiaomi_touch.c:445-469`:**
  `touch_game_mode/sensitivity` `0664`, no `capable()`. Proc mirrors gated.
- **Z7 UFS `0660` no-CAP — `drivers/scsi/ufs/ufs-mtk-dbg.c:764-861,930-943`:**
  `ufs_debug/ufs_perf` `0660` (gid 1001 `radio`), no `capable()`. Any `radio` toggles `cmd_hist` + `PERF_FORCE_ENABLE` crypto clock. UFS health `ufshcd.c:9698-9910` `0444` RO is safe.
- **Z8 KSU/SuSFS/NoMount exposure — `drivers/kernelsu/*`, `fs/susfs.c`, `fs/nomount/*`:**
  `begonia_user_ksu_defconfig:221 KALLSYMS_ALL=y`, `:255 KPROBES=y`, `:350 MODULE_SIG not set`, `:228 BPF_UNPRIV_DEFAULT_OFF` missing + `JIT_DEFAULT_ON=y`, `SLAB_FREELIST_*/REFCOUNT_FULL` not set (vs `user` hardened). `supercall.c:101-103` `reboot(0xDEADBEEF)` installs `[ksu_driver]` fd with no UID check (oracle); `dispatch.c:1201-1205 GET_INFO always_allow`. `ksu_cred:core/init.c:88,222` + `escape_with_root_profile:policy/app_profile.c:158` (`CAP_FULL_SET`+seccomp off+mount-ns). Hooks: `fs/open.c:357-378,1082-1127`, `fs/exec.c:1902-1915`, `kernel/sys.c:612-613`, `fs/namespace.c:29-326` fake `mnt_id 2000000000`, `kernel/seccomp.c:802-814` reboot bypass, `drivers/input/input.c:530-541` kprobe. `OPEN_REDIRECT:susfs.c:785-1122` + `NoMount:nomount.c:266-383` let rooted attacker spoof `mountinfo/statfs/maps/readlink` to detectors. `MODULE_SIG=n + FORCE_LOAD=y + LOCKDOWN=n` all variants = unsigned `insmod` once uid-0.
- **Z9 `dynamic_fsync` durability lie — `fs/sync.c:236-242`:**
  When enabled returns 0 without `f_op->fsync`. SQLite/OTA think committed. Default off `:25`, `0644+CAP:45-57` correct, but no per-mount opt-out / auto-off on suspend.

### F2. P2-Medium (races / parsing / overclock)

- Races: `gaming_mode_state:47` locked on write `gaming_mode.c:243` but lockless read `603-627,1898`; `pox_pwr_auto/hint/votes:62-64,483-486` unlocked; `evaluate:571-588` unlock-then-set TOCTOU; `vm_*:215-239` no sysctl lock; `cancel_delayed_work` not `_sync:470-473,702`. Anti-strobe `last_jiffies` TOCTOU (see Z2).
- Parsing: `sscanf("%d")` accepts `12abc/0x10` (`gaming_mode.c:1100,1151,1415+`); group-A silent truncate `665-931` vs group-B `-EINVAL` inconsistent; `hbm:870-886,1269-1282` no `0..3` clamp (saved downstream `leds-lm36273.c:141` but return ignored); battery/touch/audio/vib no entry clamp (downstream-only); `fops` missing `.owner:1422-1886`; sysfs `gaming_mode:1192-1195` maps `-1->0` (powersave unreachable). `EXPORT_SYMBOL` setters `:241,477,71,83,186,96,111,139` have no internal CAP.
- xswapd `mm/memcontrol.c:4087-4092` root branch counts `nr=batch` without reclaim (inflated stat); writes `4113-4182` no CAP (OK via `0664 root:system` + cgroup DAC, but missing depth); quota is trigger not limit; 128MB/work spam by `system` = CPU/IO pressure.
- Camera ISP `cam_qos.c:562-581` 560/416MHz floor every open, no thermal feedback; sensor `s5kgw1*:216-228,11280,14786,15307` 4x pixel / 3.8x MIPI, gated by `0644+CAP` now (was `0666`+autostart in `27167eac4` — verify no regress). `mt6360_strobe_store:737-769` unbounded sleep; vibrator `0x0D=3.3V:vibrator.c:102` no timeout; audio `+8dB:mt6359.c:6898` (all `0644+CAP`, bounded).
- Base partials: x_tables `net/netfilter/x_tables.c:636-655,993-1012` pad-zero done, `xt_alloc_table_info:1051` audit open; binder `drivers/android/binder.c` + CMDQ `drivers/misc/mediatek/cmdq/v3/cmdq_driver.c:1120-1143` compat passthrough + 21x `copy_from_user` / 0x `access_ok` open. `BPF_SYSCALL/JIT=y`, `USERFAULTFD=y`, `OVERLAY_FS=y` (+`REDIRECT/INDEX` on apatch/ksu only), `io_uring` absent (good), `DEVMEM/KEXEC/USER_NS` off (good).
- Build: `verity_dev_keys.x509` AOSP testkey, `DM_VERITY_AVB not set`, `PATCH_VBMETA_FLAG=auto`; USB `0666 /*/* + ttyUSB/ACM:build.sh:513-519` world-writable serial; `profile:1894-1947 0444` leaks HW state to any UID.

### F3. Patch plan (ordered, no-brick, no feature removal)

- **Phase 0 verify (no code):** as `shell(2000)`: `echo 10>/proc/perfmgr/torch_brightness` must succeed, `echo 1>/proc/perfmgr/gaming_mode` must fail; `echo 1>/proc/ktch/tb_enable`; DirtyPipe PoC on test build only; `mountinfo/maps` diff with SuSFS.
- **Phase 1 P0:** (1) `fs/pipe.c` backport `can_merge=0` + `nomerge_ops` on write + `PIPE_BUF_FLAG_CAN_MERGE` init. (2) Torch: single `atomic_long_t` + `cmpxchg` unified setter for proc/sysfs/LED, propagate `-EBUSY`, non-resettable duty-cycle + per-UID ratelimit + UID/SID `audit_log`; keep `0666` only if product demands else `0660 camera`. (3) LSM: replace basename with `kernfs_node`/attr-pointer or `(sb+ino)` + parent check, unify 4-name set, fix RCU use-after-unlock, mediate `ioctl/binder` consistently, log bypass. (4) `ktch.c:414-430` `0666->0644` + `capable(CAP_SYS_ADMIN)`.
- **Phase 2 P1:** (5) Add `capable()` to `mtk_charger.c:3446-3516,3588`, `xiaomi_touch.c:445-464`, `ufs-mtk-dbg.c:764-861`, `memcontrol.c:4113-4182`; `0664->0644` unless `system` write required. (6) KSU variant: `BPF_UNPRIV_DEFAULT_OFF=y`, `JIT_DEFAULT_ON=n`, `SLAB_FREELIST_*/REFCOUNT_FULL=y`, gate `supercall.c:101` with `allowed_for_su`, rate-limit `GET_INFO`. (7) `sync.c`: `WARN_ONCE` + auto-disable on suspend/low-battery, never auto-enable in `build.sh`. (8) `gaming_mode.c`: `kstrtoint` + trailing-junk reject + `-EINVAL` on oversize, entry clamps (`hbm 0..3` etc.), `.owner`, fix sysfs `-1`, `READ_ONCE/WRITE_ONCE` + `cancel_delayed_work_sync`.
- **Phase 3 P2+upstream:** (9) xswapd root accounting fix + quota-as-limit or rename to `trigger`. (10) Camera: thermal callback in `cam_qos.c:576` (560->416 at skin>=45C), keep `camera_4k60_force=0`, bound `strobe_store` to 500ms, vibrator timeout. (11) Full `x_tables:1051` audit, binder UAF, CMDQ `access_ok`+compat validation, `USERFAULTFD=n` if root compat allows, keep `USER_NS=n`, `KPROBES=n` on `user`. (12) Release verity key (replace testkey), `DM_VERITY_AVB=y` if needed, USB `0666->0660 system` + per-device ACLs, `profile 0444->0440`.
- Each patch: one commit per Z, `Test:` PoC before/after + boot-storm sample per `PHONE_MONITOR_REPORT.md`.

---

## G. All-branches + all-files sweep 2026-10-09 (verified via `git show <branch>:<file>`)

### G0. Branch topology

- Tips: `granite 79c65f2a2`, `main 0cdb40b8f`, `obsidian dc74d364b`, `onyx d436a6765`, `onyx-apatch 9fb74e072`, `onyx-kaeru 93b41a139`, `onyx-ksu-next 41cead4de`, `onyx-resukisu a2e52e114` (current), `hos3-fix 809a6d0ff`.
- `ff01e3cca` (x_tables/rawmidi/overlayfs) + `5eb81af82` (MAC/DAC+KASLR) + `801fede08` (xswapd/UFS) contained only in `onyx*` + `hos3-fix`. `granite/main/obsidian` do NOT contain them.
- `Makefile:2-5` `4.14.357` identical all branches. `fs/pipe.c:250 can_merge=1` identical all branches (DirtyPipe open everywhere).

### G1. `granite == main == obsidian` (stock, no Pox surface, but unpatched upstream)

- `drivers/misc/mediatek/performance/gaming_mode.c`: **absent** (`fatal: exists on disk, but not in <branch>`). Only `perfmgr_main.c` + `mtk_perfmgr_internal.h`. Zero `/proc/perfmgr/*` attack surface.
- `security/selinux/hooks.c`: no `is_rootless_allowed_node`, no `torch` match (stock, 184395B vs `onyx` 186054B). `fs/proc/inode.c`: no `S_PRIVATE/torch` match. Full DAC+MAC.
- `fs/sync.c`: 369 lines, no `dynamic_fsync` (stock). `mm/memcontrol.c`: 0x `xswapd`. `mtk_charger.c`/`mtk_battery.c`: no `pox_battery/bypass/limit`, only stock `CHARGE_CONTROL_LIMIT`. `ufs-mtk-dbg.c:722,729,735` only `debug/help/perf(0660)`, no `ufs_health 0444`.
- Defconfigs: only 5 (`begonia_apatch/user/cuttlefish/defconfig/stock`), no `begonia_user_ksu_defconfig`. `CONFIG_KSU` absent; `drivers/kernelsu/` 0 files (symlink only). `KPROBES not set` even on apatch; no `OVERLAY_REDIRECT/INDEX`.
- Hardening: `BPF_UNPRIV_OFF/SLAB_RANDOM-HARDENED/REFCOUNT/FORTIFY/YAMA/INIT_ON_ALLOC` all off, `INIT_STACK_NONE=y`, apatch `RANDOMIZE_BASE` off. **Most exposed to upstream CVEs** (`copy_up.c` no `CAP_FOWNER`, `x_tables.c` no pad-zero, `rawmidi.c` no `EBUSY`).
- BUT they still carry the MediaTek vendor debt below (G3), including `0666 torchbrightness` DAC-only (`flashlights-mt6360-mt6785.c:859` `mode 0666`, 0x `capable`) — MAC still enforces there.

### G2. Root variants (`onyx` family + `hos3-fix`, `ksu-next`, `resukisu`)

- `gaming_mode.c` + torch `0666` + dual LSM/`S_PRIVATE` bypass: **identical all 6** (`gaming_mode.c:1321,1327,2056-2060`, `hooks.c:1723,1798,3264`, `flashlight:942-950 0666`).
- `xswapd` (`memcontrol.c:4060`, `memcontrol.h:284-290`) + UFS health (`ufs-mtk-dbg.c:952 0444`, `ufshcd.c:9698-9910`) present all 6 (commits `809a6d0ff`, `44a3fe26c`).
- Charger 18W (`mtk_dual_switch_charging.c:94-95,335-341,910`, `ufshcd.h:1027`): `onyx*` only, absent `hos3-fix`.
- `drivers/kernelsu/`: 0 files `onyx/onyx-apatch/onyx-kaeru/hos3-fix`; 88 files `onyx-ksu-next` (`Kbuild:101-117` `v3.3.0` fallback `33214`, `setup.sh OWNER=KernelSU-Next`); 108 files `onyx-resukisu` (`Kbuild:54-78` `ReSukiSU v4.2.0/40901`). `fs/susfs.c` (1561L) + `fs/nomount/*` only `resukisu`; `nomount/` only `ksu-next`+`resukisu`.
- Defconfigs: `onyx/onyx-apatch/onyx-kaeru` share identical blobs; default `user` is strict-unrooted (`b0469b09b`: `KALLSYMS/KPROBES/REDIRECT not set`), `apatch` is `KALLSYMS=y,KPROBES=y,REDIRECT/INDEX/XINO=y`. `onyx-ksu-next user/apatch` add `KSU=y+NOMOUNT=y` with `KALLSYMS=y` but `KPROBES not set` (deliberate `1166d974d`). `hos3-fix` both `KALLSYMS=y,KPROBES not set`. **`onyx-resukisu begonia_user_ksu_defconfig:221,255,228,249-250,332` is most exposed**: sole `BPF_UNPRIV_DEFAULT_OFF not set` + `JIT_DEFAULT_ON=y` + `KPROBES=y` + `KALLSYMS_ALL=y` + `SLAB_*/REFCOUNT off` + `KSU_DEBUG=y` + `OVERLAY_REDIRECT=y` + full `SUSFS_*`. All 6: `MODULE_SIG not set` + `FORCE_LOAD/UNLOAD=y` + `LOCKDOWN not set` = unsigned `insmod` once uid-0.
- Unique: `kaeru 93b41a139:build.sh:65-67,358-359` bootloader/merged-DTB only, no kernel delta. `ksu-next 179552196/1166d974d`: `hook/arm64/syscall_hook.c:13,52,88` dispatcher + `supercall.c:93-100` kprobe on `REBOOT_SYMBOL` (`0xDEADBEEF/0xCAFEBABE`, `uapi/supercall.h:13-14`) + unauth `sys_reboot` magic surface. `resukisu d956d117f`: `mount.h:72-74 susfs_mnt_id_backup` + `sched.h:1411-1413 susfs_last_fake_mnt_id` persistent fake-mnt state + `OPEN_REDIRECT:785-1122` + `SPOOF_UNAME/CMDLINE`.

### G3. Repo-wide pattern scan (`onyx-resukisu` HEAD, applies to all branches unless noted)

- **TIER 0 UNPRIV (any app):**
  - `drivers/misc/mediatek/tkcore/core/tee_procfs.c:589 teed_version_write` — proc `0666 :669`, no `capable()`, `copy_from_user` ret mis-checked (`r<0` never true), OOB `teed_version[count+1]` on 51B buffer. Heap/global overflow. Present `granite` too (verified).
  - `drivers/misc/mediatek/usb20/mt6768/usb20_otg_if.c:1346` (+`mt6765:1351`) `musb_otg_test_dev.mode=0666` + `:1326 musb_otg_test_ioctl -> musb_otg_exec_cmd(cmd):1092` — raw `u32 cmd`, no `capable()`, no `compat_ioctl`. USB HW test injection. `:1300 musb_otg_test_write` `get_user` 1B drives `STOP/INIT`.
  - Torch `gaming_mode.c:910,1295` + `flashlights-mt6360-mt6785.c:839,918,949` (see Z2). `EXPORT_SYMBOL` setters `:190,:914` no CAP.
  - `tchbst/kernel/ktch.c:153,201,249 + :414,418,423,427` `tb_enable/core/freq/clstr 0666`, no `capable()` (see Z3).
  - Mali `kbdev->mdev.mode=0666` x11 — `drivers/misc/mediatek/gpu/gpu_mali/{bifrost,valhall}/mali-r{14p0:3919,15p0:3895,16p0:4041,18p0:4340,19p0:4364+4311,20p0:4396,21p0:4775,24p0:3986,25p0:4378+4333}/.../mali_kbase_core_linux.c` — `/dev/mali0` ioctl/GPU-MMU reachable by any app (known CVE class).
- **TIER 1 ioctl, no `capable()` (SYSTEM/camera-gated `/dev`, compat passthrough):**
  - `mdp/mdp_ioctl_ex.c:585` via `mdp_driver.c:1104` / `cmdq/v3/cmdq_driver.c:1091` — `copy_from_user(mdp_submit)` -> CMDQ DMA phys-reg write. `cmdq_ioctl:v3:1043` no CAP; `cmdq_ioctl_compat:1121` + `mdp_driver:1140` passthrough.
  - `m4u/2.0/m4u.c:2144 MTK_M4U_ioctl` (ALLOC_MVA/DEALLOC/CACHE/DMA) + `:2536 COMPAT` forward — IOVA map primitive, 0x `capable()`.
  - `imgsensor/src/common/v1_1/imgsensor.c:1912 compat:2016 default passthrough` — sensor I2C arbitrary reg via `FEATURECONTROL:948-969,1362-1404`.
  - `flashlight-core.c:740 _flashlight_ioctl` (`SET_DRIVER/SCENARIO/DUTY/ONOFF`) + `:948` blind `compat_ptr` forward — no CAP.
  - `ufs-mtk.c:1682 FFU` (firmware flash, permanent-brick) + `:2004 RPMB` + `:1801 query`/`:1560 fw_ver` (`buf_ptr` arbitrary) — `sg` ROOT-ONLY but impact maximal.
  - `battery_begonia/mtk_battery.c:3846 adc_cali_ioctl` (no CAP; `Slop:3873`, `Offset:3889`, `Cal:3899`, `CARTUNE:4094 500-1500`, `DISABLE_NAFG:4075` kills FG safety) + `:3802 compat` allowlist + bogus `sizeof(arg)!=8`. `charger_begonia/mtk_charger.c:832 charger_ftm_ioctl` is clean (1-cmd allowlist).
- **TIER 2 sysfs/proc stores without `capable()` (`0664/0660/0644`):**
  - `mtk_battery.c:3183 FG_daemon_disable (0664:3194)` — no parse at all, kills FG + forces `CAPACITY=50`. `:3603 BAT_EC -> :2165 exec_BAT_EC` (force-temp 101 / force-RAC 102, `strncpy+kstrtouint`). `:3207 meter_resistance` (`kstrtoul` ret ignored, `if(val<0)` dead on `unsigned long`), `:3249 nafg_disable`, `:3298 ntc_disable`, `:3354 uisoc_update`, `:3509 reset_cycle`, `:3555 reset_aging`, `:1326 Battery_Temperature` (spoof `fixed_bat_tmp`), `:1370 UI_SOC` (spoof `fixed_uisoc`).
  - `mtk_charger.c:3157 input_current`, `:3190 chg1`, `:3237 chg2`, `:3249 BatNotify`, `:3278 BN_TestMode`, `:1273 pe20` (`0664`, `kstrtouint(...,16)` ret ignored) — thermal current-limit override.
  - `thermal/common/mtk_change_policy.c:54 _tp_pid_write + :230 tp_pid 0664` (`kstrtouint` ret only `WARN_ONCE`, attacker sets thermal-daemon pid -> `send_sig_info(SIGIO)` target) + `:241 tp_test 0664`. `thermal/common/mtk_thermal_platform.c:375 validation_wr + :422 0644` toggles `check_dmips_limit` (throttling off), no CAP.
  - `ufs-mtk-dbg.c:764 debug_write + :821 perf_write (0660)` (see Z7).
- **TIER 3 ROOT-ONLY / depth:** `mtk_charger.c:3331,3380,3416` + `mtk_thermal_platform.c:383` + `battery:1274` proc writes are `0644`+daemon, clamped — OK. `thermal_zones/mtk_ts_cpu.c:1201` + `mtk_ts_wmt.c:1334` 14-arg `sscanf` ret unchecked — trip confusion only.
- Non-issues: `gaming_mode.c` all other 22 stores have `capable()` (`:672,750,803,859,1085,1136,1186,1216,1250,1275,1344,1369,1406,1447...`); `power/supply/{axp288,da9030} debugfs 0666` not compiled on MTK.

### G4. Patch deltas per branch

- `granite/main/obsidian`: apply TIER 0-2 vendor fixes (TEE bounds+`capable`, USB `0666->0660`+`capable`+`compat_ioctl`, Mali `0666->0660` + SELinux `untrusted_app` deny, battery `0664->0644`+`capable`+ret checks, thermal `+capable`, M4U/MDP/CMDQ/imgsensor/flashlight `+capable(CAP_SYS_ADMIN)` + compat validation, charger current `+capable`) + upstream backports (DirtyPipe, x_tables, rawmidi, overlayfs, binder, CMDQ `access_ok`). No torch/KSU work needed (no surface).
- `onyx*`: all above + F3 Phase 1-3 (torch unification, LSM pointer check, `ktch 0666->0644`, charger/touch/UFS/xswapd `+capable`, camera thermal callback, verity release key, USB `0666->0660`).
- `ksu-next/resukisu/apatch`: all above + lock `user_ksu` defconfig to `BPF_UNPRIV_OFF=y,JIT_DEFAULT_ON=n,SLAB_*/REFCOUNT=y,KSU_DEBUG=n`, gate `supercall.c:101` + `GET_INFO`, audit `syscall_hook.c` + `REBOOT_SYMBOL` kprobe, remove or `0600` `OPEN_REDIRECT/SPOOF` debug knobs on user builds.
