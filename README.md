# rirc.vxp

A small, keypad-driven **IRC client for MRE (`.vxp`) phones** such as the Nokia 225 (Series 30+),
inspired by the terminal IRC client [rirc](https://github.com/rcr/rirc).

> Status: first version. It has **not been compiled or run on a phone** by the author of this
> change, because the proprietary MRE SDK / TinyMRE SDK is not available in the automated
> environment. The portable IRC core is covered by host unit tests; the MRE-specific code
> (`main/`, `net/`) was only syntax-checked against stub headers. See [Validation](#validation).

## Scope

- Plain **IRC over TCP, port 6667 only**. The port is a compile-time constant
  (`RIRC_IRC_PORT` in `include/rirc_config.h`) and is not configurable.
- **No TLS** (so no port 6697), **no Bluetooth/OBEX**, no POSIX sockets.
- Features: configure server / nickname / optional channel, connect and disconnect,
  `NICK` + `USER` registration, automatic `PONG` for server `PING`, auto-join of the
  configured channel, display of channel/private/notice/server messages, joins, parts,
  quits, nick changes, kicks and topics, sending chat text and a few slash commands,
  and connection/error status.
- One scrollback for everything (server + all channels). Messages are sent to the current
  channel; messages from other channels/queries are prefixed with `[#chan]` / `[pm]`.

### Nokia 225 / MRE constraints

- Fixed-size buffers only, no allocation after start-up. Limits live in `include/rirc_config.h`:
  100 lines of scrollback, 255 characters per stored line, 512-byte IRC lines,
  2 KB output queue, 300 characters per typed message. Over-long incoming lines are dropped;
  long incoming messages are truncated for display.
- Text is shown as printable ASCII; other bytes (including UTF-8) are shown as `?`.
- No filesystem, so **settings are kept in RAM only** (see [Configuration](#configuration)).
- The connection is made through the phone's data connection (MRE access point `1`, as in the
  reference projects). Your SIM/operator must allow GPRS/EDGE data and plain TCP to port 6667.
- Plain-text IRC is **not encrypted**; do not send passwords or private data.
- The app is not a background app: when MRE suspends it (incoming call, switching apps)
  the connection may be dropped.

## Project layout

| Directory | Contents |
|-----------|----------|
| `main/` | MRE entry point (`main.c`), application glue (`app.c`), keypad UI + editor (`ui.c`) |
| `irc/` | Portable protocol code: message parser, scrollback ring, event-driven client state machine (no MRE/POSIX dependencies) |
| `net/` | MRE TCP adapter (`vm_tcp_connect` + callbacks, pattern from IPoverObexVxp) |
| `include/` | `rirc_config.h` limits and defaults |
| `resources/` | Resource pack (`.res`) packaged into the `.vxp` |
| `cmake/`, `common/`, `mreapi/`, `run/` | MRE build scaffolding taken from [gpt.vxp](https://github.com/RDZDX/gpt.vxp) / [IPoverObexVxp](https://github.com/RDZDX/IPoverObexVxp) |
| `tests/` | Host unit tests for `irc/` |
| `docs/` | Design notes |

## Prerequisites and toolchain

Inherited unchanged from the sample projects:

- CMake >= 3.10 and a Make generator
- `arm-none-eabi-gcc` / `g++` (the toolchain file is `cmake/toolchain-arm-none-eabi.cmake`;
  override the install location with `-DTOOLCHAIN_PREFIX=...` if needed)
- The MediaTek **MRE SDK** (headers + `lib/MRE30/armgcc/*.a`), exported as `MRE_SDK`
- **TinyMRE SDK** (provides `bin/PackRes` and `bin/PackApp`), exported as `TinyMRESDK`
- For Nokia phones the `.vxp` must be signed with your SIM's IMSI, see
  [vxpatch](https://vxpatch.luxferre.top)

```sh
export MRE_SDK=/path/to/MRE_SDK
export TinyMRESDK=/path/to/TinyMRESDK
```

## Build

```sh
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-none-eabi.cmake \
      -DCMAKE_BUILD_TYPE=Release -DIMSI=<your IMSI>
cmake --build build
```
The result is `rirc.vxp` (in `build/main/`). `-DIMSI=` is baked in by `PackApp` for signing
(default `1234567890` is a placeholder). The app requests only the `TCP` MRE API.

For the MoDis/MREmu simulator use the `win32` preset (Visual Studio generator); `run/run_in_MoDis`
copies the package to the emulator and writes a launcher batch file.

### Host unit tests (no SDK needed)

```sh
cmake -S . -B build-host -DRIRC_HOST_TESTS=ON
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

## Install and run

1. Build (and sign for your IMSI) `rirc.vxp`.
2. Copy it to the phone's memory card (typically the `mre` folder) and open it from the
   phone's file manager / apps menu.
3. Open **Menu > Server / Nick / Channel** to configure, then choose **Connect**.

## Configuration

Defaults are compiled in (`RIRC_DEFAULT_SERVER`, `RIRC_DEFAULT_NICK`, `RIRC_DEFAULT_CHANNEL` in
`include/rirc_config.h`). At run time use **Menu**:

- **Server** - host name (letters, digits, `.`, `-`, `_`); port is always 6667
- **Nick** - up to 16 characters: letters, digits (not first), `[]\`_^{|}-`
- **Channel** - optional; a leading `#` is added if you omit it. Joined automatically after the
  server's welcome (`001`).

Values are validated when you press OK. Settings are not persisted across restarts.
If the nick is taken, `_` is appended automatically (up to 4 times).

## Keypad controls

### Chat view

| Key | Action |
|-----|--------|
| Left softkey / OK | `Connect` when offline, `Stop` while connecting, `Write` when connected |
| Right softkey | Menu |
| Up / Down | Scroll one row |
| Left / Right | Scroll one page |
| Digit keys (while connected) | Start writing a message with that character |

### Menu

| Key | Action |
|-----|--------|
| Up / Down | Move selection |
| Left softkey / OK | Select (Connect/Disconnect, Write, edit Server/Nick/Channel, Clear log, Help, Exit) |
| Right softkey | Back |

### Editor (multi-tap)

| Key | Action |
|-----|--------|
| `2`-`9` | Letters (press repeatedly to cycle, e.g. `2` = a b c 2) |
| `0` | Space, `0` |
| `1` | `. , ' ? ! - 1` |
| `*` | Symbols: `# / @ : _ * + = ( )` |
| `#` | Cycle mode: `abc` > `Abc` > `ABC` > `123` |
| Left softkey / OK | Send / accept |
| Right softkey | Delete last character (hold to repeat); `Back` when empty (cancel) |

Text is appended at the end; there is no cursor movement. A short pause (about one second)
commits the current multi-tap character.

### Slash commands (in the message editor)

`/join #chan`, `/part [#chan]`, `/nick name`, `/me action`, `/msg target text`, `/quit [message]`,
`/help`. Start a message with `//` to send a literal leading `/`.

## Attribution

This project is a port/re-implementation inspired by **rirc** by Richard Robbins
(<https://github.com/rcr/rirc>), MIT licensed, Copyright (C) 2014-2023 Richard Robbins.
rirc's terminal, POSIX-socket and blocking `select` event loop are not portable to MRE, so the
IRC message grammar and the behaviour of the message handling (display formatting, registration,
`PING`/`PONG`, nick collision handling, channel tracking) were re-implemented as a compact
event-driven state machine in `irc/` rather than copying rirc sources. The upstream license text
is reproduced in [`LICENSE`](LICENSE).

The MRE/CMake scaffolding (`cmake/`, `common/`, `mreapi/`, `run/`, `CMakePresets.json`) comes from
[RDZDX/gpt.vxp](https://github.com/RDZDX/gpt.vxp) and the TCP pattern in `net/` follows
[RDZDX/IPoverObexVxp](https://github.com/RDZDX/IPoverObexVxp) (by XimikBoda); the ARM toolchain file is
by Johannes Bruder (MIT). Only the TCP part was used; no Bluetooth/OBEX code is included.

## Validation

- Host tests (`tests/test_irc.c`) cover the parser, handshake, `PING`/`PONG`, nick collisions,
  message display, slash commands, injection/limit handling and the scrollback ring.
- The MRE parts (`main/`, `net/`, CMake `.vxp` packaging) could **not** be built here because the
  MRE SDK and TinyMRE SDK are proprietary and unavailable. They were only syntax-checked with
  stub headers; the SDK API usage follows the reference projects but may need small adjustments
  on the first real build. Not tested on hardware.

## License

MIT, see [`LICENSE`](LICENSE).
