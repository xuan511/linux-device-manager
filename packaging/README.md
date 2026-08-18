# Linux Deployment Examples

Core development and the PTY demo run as an ordinary user and do not need these
files. Packaging is an optional real-hardware deployment layer.

1. Install binaries with `cmake --install build/release --prefix /usr/local`.
2. Create a locked service account: `sudo useradd --system --no-create-home devmgr`.
3. Add it to the distribution's serial-access group (usually `dialout`).
4. Inspect the real adapter using `udevadm info --attribute-walk`; replace the
   deliberately invalid VID/PID placeholders and review the resulting rule.
5. Copy `devmgrd.conf.example` to `/etc/devmgrd.conf` and set paths.
6. Install the service file, run `systemctl daemon-reload`, then enable/start it.

The daemon stays in foreground; systemd owns daemonization, restart, logs, and
the runtime directory. The hardening options deny filesystem writes except the
systemd-managed runtime socket. `AF_UNIX` is the only required socket family.
Do not blindly install the udev template: permissions and identifiers are
machine-specific security decisions.

