# RTSS CAN Daemon

RTSS CAN Daemon (`rtss_can`) is a C daemon that bridges Linux SocketCAN virtual CAN interfaces and RTSS mailbox channels. The source describes it as a SocketCAN-to-RTSS mailbox bridge for CAN-FD traffic.

The checked-in `README.md` uses the repository template headings. This updated README keeps that heading structure and fills in the details that can be confirmed from the repository.

## Branches

**main**: Primary development branch. Contributors should develop submissions based on this branch and submit pull requests to this branch.

## Requirements

### Supported environment

This project is intended for Linux systems with SocketCAN and RTSS mailbox support. The specific Qualcomm processor or target platform is not specified in the repository.

The source uses Linux-specific interfaces and headers, including:

- `linux/can.h`
- `linux/can/raw.h`
- SocketCAN raw sockets
- `vcan` virtual CAN interfaces
- CAN gateway rules through `cangw`
- RTSS mailbox device paths under `/dev/rtss/`

### Build requirements

The repository uses CMake and C.

Required or implied by the build files:

- CMake 3.16 or newer for `src/CMakeLists.txt`.
- A C compiler with C11 support.
- `librt`.
- `pthread`.
- `glib-2.0`.
- Linux CAN headers (`linux/can.h`, `linux/can/raw.h`).
- `pkg-config` is optional, but CMake uses it to discover `glib-2.0` when available.

### Runtime requirements

The daemon invokes system commands to configure CAN interfaces and gateway rules. The runtime system must provide:

- `ip`, typically from `iproute2`.
- `modprobe`, typically from `kmod`.
- `cangw`, typically from `can-utils`.
- Kernel modules used by the daemon:
	- `can`
	- `can-raw`
	- `can-gw`
	- `vcan`

Creating interfaces, loading kernel modules, and setting CAN gateway rules require appropriate privileges, typically root or equivalent Linux capabilities.

The daemon also expects RTSS mailbox channels to be available:

```text README.md
/dev/rtss/can0   # shared TX channel from Linux to RTSS
/dev/rtss/can1   # shared RX channel from RTSS to Linux
```

### External RTSS mailbox dependency

The repository does not include all headers and sources needed for the RTSS mailbox API. The source includes `rtss_mailbox.h`, and `src/CMakeLists.txt` references external files under a path that still uses the upstream library's original directory name:

```text README-updated.md
${WORKDIR}/sail-mailbox/sail-mailbox-usr/sail-mb-umd/include
${WORKDIR}/sail-mailbox/sail-mailbox-usr/sail-mb-umd/src
```

Note: the directory name `sail-mailbox` is the name of the external upstream library on disk as referenced by CMake. It is not a project name used by this daemon.

The CMake variables `WORKDIR`, `SYSROOTINC_PATH`, and `SYSROOT_INCLUDEDIR` are referenced but not defined in this repository. They must be provided by the build environment or passed to CMake. Exact setup instructions for those external dependencies are not specified in the repository.

### Configuration requirements

The example configuration file is:

```text README.md
src/rtss_can.conf
```

The daemon source currently loads configuration from:

```text README.md
/etc/rtss_can/rtss_can.conf
```

Before running the daemon, ensure that a configuration file exists at the runtime path above unless the source is changed to use a different path.

The provided configuration has:

```ini README.md
active_controllers=0
```

With zero active controllers, the daemon exits during initialization. Set `active_controllers` to a value from `1` to `8` to enable controllers.

The current parser in `rtss_can.c` applies these values:

- `active_controllers=N`
- `controller_X_baudrate=Y`

Other keys in `src/rtss_can.conf` document intended settings, but the current parser does not apply them.

Supported baud-rate configuration IDs are defined in `src/rtss_can_baud_config.h`:

| ID | Nominal bitrate | Data bitrate | BRS |
| --- | --- | --- | --- |
| 0 | 500 kbps | 500 kbps | Disabled |
| 1 | 250 kbps | 5 Mbps | Enabled |
| 2 | 125 kbps | 125 kbps | Disabled |
| 3 | 1 Mbps | 1 Mbps | Disabled |
| 4 | 1 Mbps | 2 Mbps | Enabled |
| 5 | 1 Mbps | 5 Mbps | Enabled |
| 6 | 1 Mbps | 8 Mbps | Enabled |
| 7 | 1 Mbps | 10 Mbps | Enabled |
| 8 | 1 Mbps | 11.428 Mbps | Enabled |

## Installation Instructions

Because the external RTSS mailbox dependency and several CMake variables are not fully specified in this repository, the project may not build standalone immediately after cloning.

A standard out-of-source CMake build is expected to look like this once dependencies are available:

```bash README-updated.md
cmake -S . -B build \
	-DCMAKE_BUILD_TYPE=Release \
	-DWORKDIR=/path/to/workdir \
	-DSYSROOTINC_PATH=/path/to/sysroot/includes \
	-DSYSROOT_INCLUDEDIR=/path/to/sysroot/usr/include

cmake --build build
```

For a debug build:

```bash README-updated.md
cmake -S . -B build-debug \
	-DCMAKE_BUILD_TYPE=Debug \
	-DWORKDIR=/path/to/workdir \
	-DSYSROOTINC_PATH=/path/to/sysroot/includes \
	-DSYSROOT_INCLUDEDIR=/path/to/sysroot/usr/include

cmake --build build-debug
```

### CMake options and variables found in the repository

| Name | Default | Purpose |
| --- | --- | --- |
| `CMAKE_BUILD_TYPE` | `Release` if unset | Selects debug or release flags. |
| `BUILD_DOCS` | `OFF` | Attempts to build documentation with Doxygen and a `docs` subdirectory if enabled. No `docs` directory is present in the repository. |
| `USE_STUB_DRIVERS` | Not set | Adds `USE_STUB_DRIVERS=1` when set; otherwise `0`. |
| `RTSS_CAN_ENABLE_CANFD` | Not set | Adds `ENABLE_CANFD=1` or `0` when defined. |
| `RTSS_CAN_ENABLE_GATEWAY` | Not set | Adds `ENABLE_GATEWAY=1` or `0` when defined. |
| `RTSS_CAN_MAX_CONTROLLERS` | Not set | Overrides `MAX_CAN_CONTROLLERS` as a compile definition when defined. |
| `WORKDIR` | Not specified | Used to locate the external RTSS mailbox library sources and headers. The library resides in a directory named `sail-mailbox` on disk, as referenced in `src/CMakeLists.txt`. |
| `SYSROOTINC_PATH` | Not specified | Added to include directories. |
| `SYSROOT_INCLUDEDIR` | Not specified | Added to include directories. |

### Install target

The CMake install rules install:

- Executable: `${CMAKE_INSTALL_BINDIR}/rtss_can`
- Config file: `${CMAKE_INSTALL_SYSCONFDIR}/rtss_can/rtss_can.conf`

After a successful build, installation would normally be run with:

```bash README-updated.md
cmake --install build
```

The CMake install rules install the configuration file to `${CMAKE_INSTALL_SYSCONFDIR}/rtss_can/rtss_can.conf`, which on a standard Linux system resolves to `/etc/rtss_can/rtss_can.conf`. This matches the runtime path defined in `src/rtss_can_config.h` as `CONFIG_FILE_PATH`.

### Known build/configuration issues in the repository

These are visible from the current source and CMake files:

- `src/CMakeLists.txt` creates a static library target named `sail_umd` (the upstream library's own target name), but `target_link_libraries()` references `rtss_umd`. This name mismatch will cause a linker error and needs to be resolved before a clean build.
- External RTSS mailbox headers and source files are not included in this repository.
- The `--config FILE` command-line option is accepted by the argument parser, but the source still loads `CONFIG_FILE_PATH` instead of the provided file path.

## Usage

### Purpose and message flow

The daemon bridges standard Linux SocketCAN applications to RTSS mailbox channels. The high-level flow is:

```text README.md
SocketCAN applications
	-> user VCAN interfaces
	-> CAN gateway rules
	-> daemon VCAN interfaces
	-> rtss_can daemon
	-> RTSS mailbox channels
```

In more detail:

1. **SocketCAN applications** send or receive CAN/CAN-FD frames using normal Linux SocketCAN interfaces, such as `vcan0`.
2. **User VCAN interfaces** are the application-facing virtual CAN interfaces. For example, controller 0 uses `vcan0`, controller 1 uses `vcan1`, and so on.
3. **CAN gateway rules** are created with `cangw` to forward traffic between the user-facing VCAN interface and the daemon-facing VCAN interface.
4. **Daemon VCAN interfaces** are the interfaces that `rtss_can` listens on. Source comments describe the default layout as `vcan8`-`vcan15` for the daemon side, paired with `vcan0`-`vcan7` for the user side.
5. **`rtss_can` daemon** reads SocketCAN frames, converts them to RTSS CAN mailbox packets, and sends them to RTSS. It also reads RTSS mailbox packets, converts them back to SocketCAN CAN-FD frames, and writes them to the correct daemon VCAN interface.
6. **RTSS mailbox channels** are the shared device channels used for communication with RTSS: `/dev/rtss/can0` for TX and `/dev/rtss/can1` for RX.

Transmit path from Linux to RTSS:

```text README.md
Application -> vcan0 -> cangw -> daemon VCAN -> rtss_can -> /dev/rtss/can0 -> RTSS
```

Receive path from RTSS to Linux:

```text README.md
RTSS -> /dev/rtss/can1 -> rtss_can -> daemon VCAN -> cangw -> vcan0 -> Application
```

### Running the daemon

The daemon supports these command-line options according to `print_usage()` in `src/rtss_can.c`:

```text README.md
Usage: rtss_can [OPTIONS]
CAN Mailbox Daemon - Bridges SocketCAN and RTSS mailbox.

Options:
	-d, --debug          Enable debug mode
	-c, --config FILE    Configuration file path
	-h, --help           Show this help message
```

Current behavior from the source:

- `--debug` enables debug mode, console logging, and debug-level logs.
- `--help` prints usage and exits.
- `--config FILE` is parsed but not used when loading configuration.

Example:

```bash README-updated.md
./build/src/rtss_can --debug
```

The exact executable path depends on the build and install location.

### Runtime configuration reload

The daemon handles `SIGHUP` by reloading the configuration file and resending baud-rate configuration for enabled controllers:

```bash README-updated.md
killall -HUP rtss_can
```

### Sending traffic through SocketCAN

For each enabled controller, applications use the user-facing VCAN interface. For example, after the daemon has created interfaces and gateway rules, controller 0 traffic is sent through `vcan0`.

Example using `can-utils`, if available:

```bash README.md
cansend vcan0 123##011223344
```

The repository does not include a complete end-to-end runtime example beyond the command-line usage printed by the daemon.

### Logging

The logging system supports:

- Levels: `DEBUG`, `INFO`, `WARN`, `ERROR`, `FATAL`
- Destinations: console, syslog, file
- CAN frame logging
- CAN-FD frame logging
- Mailbox packet logging

The default runtime logging configuration in `parse_command_line()` is syslog at `INFO` level. Debug mode adds console logging and enables debug-level logs.

## Development

### Repository structure

```text README.md
.
├── CMakeLists.txt                  # Top-level CMake entry point; adds ./src
├── README.md                       # Existing template README
├── README.md                       # Updated documentation
├── LICENSE.txt                     # BSD-3-Clause license text
├── CONTRIBUTING.md                 # Contribution guide; still contains template placeholders
├── CODE-OF-CONDUCT.md              # Contributor Covenant code of conduct
├── SECURITY.md                     # Vulnerability reporting guidance; partially templated
├── .github/
│   ├── dependabot.yaml             # Dependabot config for GitHub Actions
│   ├── workflows/
│   │   ├── qcom-preflight-checks.yml
│   │   ├── stale-issues.yaml
│   │   └── Readme.md
│   ├── ISSUE_TEMPLATE/
│   └── PULL_REQUEST_TEMPLATE/
└── src/
		├── CMakeLists.txt              # Main build configuration
		├── rtss_can.c                  # Main daemon implementation
		├── rtss_can.conf               # Example/default daemon configuration
		├── rtss_can_config.h           # Platform constants and runtime paths
		├── rtss_can_structures.h       # CAN mailbox packet structures
		├── rtss_can_baud_config.h      # Supported baud-rate configuration IDs
		├── rtss_can_logging.c          # Logging implementation
		├── rtss_can_logging.h          # Logging API
		├── rtss_mb_wrapper.c               # RTSS mailbox wrapper implementation
		└── rtss_mb_wrapper.h               # RTSS mailbox wrapper API
```

### Key source files

- `src/rtss_can.c`: daemon main loop, command-line parsing, configuration loading, VCAN setup, SocketCAN handling, RTSS mailbox forwarding, signal handling, and cleanup.
- `src/rtss_mb_wrapper.c`: wrappers around RTSS mailbox operations for shared TX/RX channels.
- `src/rtss_can_logging.c`: syslog, console, and file logging implementation.
- `src/rtss_can_baud_config.h`: supported CAN-FD baud-rate configuration IDs.
- `src/rtss_can_structures.h`: mailbox packet layouts.

### Runtime resources created by the daemon

On startup, the daemon may create:

- User VCAN interfaces for enabled controllers.
- Daemon VCAN interfaces for enabled controllers.
- Bidirectional CAN gateway rules.
- Shared RTSS mailbox clients.
- An RX thread for RTSS-to-SocketCAN forwarding.

On shutdown, it attempts to close sockets, stop the RX thread, close mailbox clients, flush CAN gateway rules, and delete the VCAN interfaces it created for enabled controllers.

### Signals

- `SIGINT` and `SIGTERM`: request graceful shutdown.
- `SIGHUP`: reload configuration and reapply baud-rate settings.

### Testing

No test framework, test directory, or test scripts are included in the repository.

The GitHub workflow `.github/workflows/qcom-preflight-checks.yml` runs Qualcomm reusable preflight checks on pull requests and pushes to `main`, including:

- Semgrep scan
- Dependency review
- Repolinter check
- Copyright/license check
- Commit email check

These workflow checks are CI/preflight checks, not project unit or runtime tests.

### Packaging

`src/CMakeLists.txt` includes CPack configuration:

- Package name: `rtss_can`
- Package version: `1.0.0`
- DEB dependencies: `libc6, can-utils`
- RPM requirements: `glibc, can-utils`

Example packaging command after a successful CMake configure/build:

```bash README.md
cpack --config build/CPackConfig.cmake
```

Note: Package generation has not been verified from repository contents, and external dependencies may still be required.

### Contributing

See [`CONTRIBUTING.md`](CONTRIBUTING.md) for contribution guidance. That file currently contains template placeholders, but it specifies:

- Develop branches from `main`.
- Submit pull requests against `main`.
- Read the code of conduct and license.
- Sign commits using the Developer Certificate of Origin (`git commit -s`).
- External pull requests are scanned with Semgrep.

Also see [`CODE-OF-CONDUCT.md`](CODE-OF-CONDUCT.md).

## Getting in Contact

The repository does not specify project maintainer names, maintainer email addresses, or a project-specific discussion forum.

Available contact and reporting channels found in the repository:

- [Report an Issue on GitHub](../../issues)
- Security issues: see [`SECURITY.md`](SECURITY.md)
- Code of conduct reports: `github.coc@qti.qualcomm.com`, as listed in [`CODE-OF-CONDUCT.md`](CODE-OF-CONDUCT.md)


## License

RTSS CAN Daemon is licensed under the BSD-3-Clause License. See [`LICENSE.txt`](LICENSE.txt) for the full license text.

The license file contains:

```text README-updated.md
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
```
