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
