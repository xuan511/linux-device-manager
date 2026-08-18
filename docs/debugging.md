# Debugging Guide

## Build for Inspection

```sh
cmake --preset debug && cmake --build --preset debug -j
gdb --args build/debug/devmgrd --device /dev/pts/5 --socket /tmp/devmgrd.sock
```

Useful breakpoints: `devmgr_parser_feed` for stream slices,
`devmgr_frame_decode` for CRC/header failures, `devmgr_session_tick` for retry,
`devmgr_upgrade_accept_response` for upgrade edges, `on_device_frame` for
sequence routing, and `cleanup_context` for shutdown ownership.

## Observe System Calls and FDs

```sh
strace -ff -tt -e trace=epoll_wait,read,write,timerfd_settime,signalfd4,close \
  build/debug/devmgrd --device /dev/pts/5 --socket /tmp/devmgrd.sock
ls -l /proc/$(pgrep -n devmgrd)/fd
cat /proc/$(pgrep -n devmgrd)/fdinfo/3
```

After SIGTERM, confirm the process and socket disappear. A noncanonical TTY may
return zero after ready bytes are drained; inspect epoll HUP/ERR separately from
that empty read.

## Protocol / CRC

Enable or add a temporary hex dump immediately before `devmgr_parser_feed` and
after `devmgr_frame_encode`. Decode bytes 0..11 using the table in protocol.md;
calculate CRC over header+payload only. Known input `123456789` must yield
`CBF43926`. Seed fault injection so a failure is repeatable.

## Memory and Undefined Behavior

```sh
./scripts/run-sanitizers.sh
./scripts/run-valgrind.sh
```

Do not suppress a report before proving it belongs to the runtime. For a parser
crash, save the exact byte slice and turn it into a unit vector or libFuzzer
corpus entry.

## Installed Service

```sh
journalctl -u devmgrd -f
systemctl show devmgrd -p MainPID -p SubState -p RestartUSec
udevadm monitor --udev --property
```

The packaged udev file is intentionally invalid until real VID/PID values are
reviewed. Hardware UART and STM32 flash/jump debugging remain physical tasks.

