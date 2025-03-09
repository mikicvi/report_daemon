# Report Daemon

A system daemon that monitors, moves, and backs up department reports.

## Prerequisites

- Linux system with systemd
- GCC compiler
- Root/sudo access

## Installation & Setup

Build the daemon:
```sh
make
```

Install and configure:
```sh
sudo make install
```

Start the service:
```sh
sudo make start
```

## Directories

| Path | Purpose |
|------|---------|
| `/var/reports/uploads` | Watch directory for incoming reports |
| `/var/reports/reporting` | Processed reports storage |
| `/var/reports/backup` | Backup archive location |
| `/var/log/report_daemon.log` | Log file |

## Development

Monitor IPC messages (debugging):
```sh
make monitor
```

## Cleanup

Remove daemon and configuration:
```sh
sudo make uninstall
```

Clean build artifacts:
```sh
make clean
```