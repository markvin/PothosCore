# Phase 17: PothosAudio on the pothos2 core with miniaudio

Date: 2026-10-31. Status: design. The work happens in a new fork
`markvin/PothosAudio`, then in `markvin/PothosBase`. The core does not
change, except its docs.

## Intent

PothosAudio gives the audio blocks of Pothos: a source that reads an
audio input device into a sample stream, and a sink that writes a sample
stream to an audio output device. The user wants this graph in
PothosFlow: a microphone or a line input → the speakers (2026-10-31).

Upstream PothosAudio uses PortAudio. The user decided to replace
PortAudio with miniaudio, and to keep a copy of `miniaudio.h` in the
toolkit (2026-10-31).

When this phase is complete, audio builds against an installed pothos2
core (version 2.0) with a copy of miniaudio, has no Poco and no compiler
warnings in its own code, has self-tests that need no audio hardware,
has CI, and is a submodule of PothosBase.

## Decisions of the user

1. Port PothosAudio, with miniaudio in place of PortAudio (2026-10-31).
2. The use case: a microphone or a line input → the speakers, in a graph
   of PothosFlow (2026-10-31). This is also the manual check of the user.
3. miniaudio is a copy of `miniaudio.h` in the toolkit, not a package
   from `find_package` (2026-10-31). This is an exception to the rule
   "dependencies only from `find_package`" of the other toolkits:
   miniaudio is a single-file library, it is made to be copied, and Arch
   has no official package of it.

## Current state (recon on upstream `a68f586`, 2026-10-31)

- One module `AudioSupport`, 5 source files, about 580 lines:
  `AudioBlock.hpp/.cpp` (the common base), `AudioSource.cpp`,
  `AudioSink.cpp`, `AudioInfo.cpp`.
- `/audio/source` and `/audio/sink`: `make(dtype, numChans, chanMode)`.
  - `dtype`: `float32`, `int32`, `int16`, `int8`, `uint8`.
  - `chanMode`: `INTERLEAVED` (one port with a vector type of `numChans`
    elements) or `PORTPERCHAN` (one port per channel).
  - Calls: `setupDevice(deviceName)` (empty: the default device; digits:
    a device index; else a device name), `setupStream(sampRate)`,
    `setReportMode(mode)` (`LOGGER`, `STDERROR`, `DISABLED`),
    `setBackoffTime(ms)`, `overlay()` (a JSON overlay: an editable
    drop-down list of the device names for PothosFlow).
  - The source posts the label `rxRate` (the sample rate) on its outputs
    in the first `work()` after `activate()`.
  - At an overflow (source) or an underflow (sink), the block writes
    `aO` or `aU` to stderr (`STDERROR`) or an error to the logger
    (`LOGGER`), and waits for the backoff time before it produces or
    consumes again.
  - The `|PothosDoc` comments give the GUI parameters (default sample
    rate 44.1e3, the options 32e3, 44.1e3, 48e3).
- `/devices/audio/info`: a JSON dump of the devices (keys "PortAudio
  Device" with "Device Name", "Host API Name", "Max Input Channels",
  "Max Output Channels", "Default Sample Rate"; "PortAudio Version").
- PortAudio is used in blocking mode (`Pa_ReadStream`,
  `Pa_WriteStream`, `Pa_GetStreamReadAvailable`), with
  `MIN_FRAMES_BLOCKING=1024` when no frames are available.
- Poco: only `Poco::Logger` in `AudioBlock`. nlohmann json for the
  overlay and the info.
- CMake 2.8.9, `FindPortaudio.cmake`, `ENABLE_AUDIO` with
  `cmake_dependent_option` (a missing PortAudio turns the module off),
  `find_path(json.hpp)`, links the target `Pothos`. `debian/`. No
  tests.

## Goals

- The fork `markvin/PothosAudio`, branch `pothos2`, builds against an
  installed pothos2 core (2.0) with CMake 3.25 or newer (also CMake 4),
  C++23.
- No PortAudio and no Poco. miniaudio from the copy in the toolkit.
- No compiler warnings in the toolkit code with `-Wall -Wextra` (GCC and
  Clang). `AUDIO_WARNINGS_AS_ERRORS=ON` turns them into errors.
- The block API stays the same: the paths, the `make` arguments, the
  calls, the `rxRate` label, the report modes, the `|PothosDoc`
  parameters. A topology of upstream PothosFlow that uses the audio
  blocks opens and works when it uses the default device (an empty
  device name). A saved PortAudio device name (for example `hw:0,0` or
  `pulse`) or a PortAudio index does not match the miniaudio list: the
  user selects the device again (my call, refer to `setupDevice`).
- Self-tests without audio hardware pass on Linux, macOS and Windows in
  CI.
- The manual check of the user passes: a microphone or a line input →
  the speakers in PothosFlow, on the computer of the user (Linux).
- PothosBase builds audio as a submodule and runs its self-tests.

## Non-goals

- No new features: no choice of the backend in the GUI, no exclusive
  mode, no new block parameters, no new sample formats.
- No miniaudio 0.12 (in development at this time).
- No packages (Debian, Conan package, installer).
- No change of the core, except its docs.

## Anti-goals

These results are failures, also when the build is correct:

- The configure step continues without a part and gives a build without
  the module.
- The tests need a sound card, a sound server or a display, or they pass
  because they skip a check without a message.
- The build needs the development packages of ALSA, PulseAudio, JACK,
  CoreAudio or WASAPI that the old build did not need. (miniaudio loads
  the Linux backends at run time.)
- The audio callback blocks (a lock that `work()` can hold for a long
  time, a memory allocation, a log call in the callback).
- A CMake file refers to Conan, or the configure or build step downloads
  files.
- audio builds in PothosBase but not standalone against an installed core
  (or the opposite).

## Repository and history

1. Fork `pothosware/PothosAudio` to `markvin/PothosAudio`. The user
   creates the fork (the auto-mode classifier refuses `gh repo fork`).
2. Make the branch `pothos2` from upstream `master` (`a68f586`). Upstream
   history stays the ancestor.
3. The local clone is `../PothosAudio` (it exists, upstream clone). Set
   `origin` to the fork (SSH) and `upstream` to pothosware.
4. The work goes into a work branch and a PR into `pothos2` of the fork.
   The user approves the first push and each merge.

## miniaudio (decision 3)

- Copy `miniaudio.h` of the latest miniaudio 0.11 release (0.11.25 or a
  newer 0.11 release at the time of the work) into
  `third_party/miniaudio/`, with its license file (`LICENSE`, public
  domain or MIT No Attribution) and a `README.md` that gives the version,
  the source URL, the SHA-256 of the file, and how to update it. Get the
  file by hand from the GitHub release of miniaudio; the build never
  downloads it.
- Compile the implementation (`#define MINIAUDIO_IMPLEMENTATION`) once,
  in its own source file, in an internal static library or object
  library of the toolkit. That file is third-party code: it gets no
  `-Wall -Wextra -Werror` from the toolkit, and its include directory is
  a `SYSTEM` include directory for the toolkit code. Decision of the
  builder: a C or a C++ translation unit.
- Configure miniaudio with defines on that target only, as small as
  possible: no decoding, encoding, generation, resource manager, node
  graph or engine (`MA_NO_DECODING`, `MA_NO_ENCODING`,
  `MA_NO_GENERATION`, `MA_NO_RESOURCE_MANAGER`, `MA_NO_NODE_GRAPH`,
  `MA_NO_ENGINE`), only the device I/O.
- Link the system libraries that miniaudio needs: `Threads::Threads` and
  `${CMAKE_DL_LIBS}` (and `m`) on Linux; the frameworks CoreFoundation,
  CoreAudio and AudioToolbox on macOS (no Objective-C, no AVFoundation);
  nothing extra on Windows. Decision of the builder: the exact list after
  a build on each platform.
- On Linux, miniaudio loads ALSA, PulseAudio and JACK at run time. The
  build needs no audio development packages. PipeWire works through its
  PulseAudio or ALSA compatibility.

## Design of the blocks

- `AudioBlock` keeps the role of the common base. A miniaudio context
  (`ma_context`) for each block (or one shared context with a reference
  count; decision of the builder), and one `ma_device` (capture for the
  source, playback for the sink).
- `setupDevice(deviceName)`:
  - Empty name: the default capture or playback device.
  - A name of digits: the index in the list of `ma_context_get_devices`
    (capture list for the source, playback list for the sink). Out of
    range: `Pothos::RangeException`.
  - Else: the device with that name in the list. No match:
    `Pothos::NotFoundException`.
  - No device at all: `Pothos::NotFoundException`.
- `setupStream(sampRate)`: initialize the device with the format of
  `dtype`, `numChans` channels and the sample rate; a failure gives a
  `Pothos::Exception` with the miniaudio error text
  (`ma_result_description`). Log the device name and the backend name at
  the Information level, as before.
- Conversion (my call): miniaudio converts the format, the channel count
  and the sample rate between the device and the block. Thus a request
  that the hardware does not support works (for example 44.1e3 on a
  48 kHz device, or 2 channels from a mono microphone). PortAudio on a
  sound server converted too. Log the internal format, channel count and
  rate of the device at the Information level when they differ from the
  request.
- Unknown device name (my call): an exception, not a fallback to the
  default device. A silent change of the device is worse than a clear
  error in an old topology.
- Data path: the device callback and `work()` share a lock-free ring
  buffer (`ma_pcm_rb`, single producer, single consumer). Decision of the
  builder: the buffer size (a fixed number of periods, for example 4 ×
  the period of the device or about 100 ms; it must hold at least
  `MIN_FRAMES_BLOCKING` frames).
  - Source: the callback writes the captured frames into the buffer. A
    full buffer is an overflow: the callback drops the frames and sets an
    atomic flag. `work()` reads the available frames (up to the output
    space) and reports the overflow flag as before (`aO` or the logger,
    and the backoff time). When no frames are available, `work()` waits
    a short time for data (as the blocking read did) or yields; it must
    not spin at full CPU and must not block deactivate.
  - Sink: `work()` writes the input frames into the buffer (up to the
    free space). The callback reads the frames; an empty buffer is an
    underflow: the callback outputs silence for the missing frames and
    sets an atomic flag. `work()` reports it as before (`aU`). The same
    rules for the wait.
  - Sink start (my call): the callback counts no underflow before the
    first frames that `work()` wrote after `activate()`. Thus a start
    does not report `aU` and does not start the backoff time. After that
    first write, an empty buffer is an underflow, also when the upstream
    is slow or has stopped.
  - The callback does no allocation, takes no lock that `work()` holds,
    and does not log.
- `PORTPERCHAN`: miniaudio gives interleaved frames only. The block
  deinterleaves (source) or interleaves (sink) between the ring buffer
  and the ports.
- `int8`: miniaudio has no signed 8-bit format. The block uses
  `ma_format_u8` and converts each sample (`x ^ 0x80`) in `work()`. The
  other types map directly: `float32` → `ma_format_f32`, `int32` →
  `ma_format_s32`, `int16` → `ma_format_s16`, `uint8` → `ma_format_u8`.
- `rxRate`: the source posts the sample rate of the data that the block
  produces, that is the requested rate (`ma_device.sampleRate`), not the
  internal rate of the hardware.
- `activate()` starts the device, `deactivate()` stops it, and the
  destructor uninitializes the device and the context. A failure in
  `deactivate()` gives an exception, as before.
- `overlay()`: the same JSON structure (an editable drop-down list "Default
  Device" and the device names), from the miniaudio device list.
- `/devices/audio/info`: the same structure with miniaudio values. My
  call for the keys: "Audio Device" (with "Device Name", "Backend Name",
  "Max Input Channels", "Max Output Channels", "Default Sample Rate"),
  and "miniaudio Version". The values that miniaudio does not give
  directly come from `ma_context_get_device_info`; a value that is not
  known is left out.
- Logging: `POTHOS_LOG_*` with the source names `AudioSource` and
  `AudioSink`, as before.

## Backend for the tests

- The tests need a device without hardware. miniaudio has a null backend
  (`ma_backend_null`): a virtual device that gives silence and consumes
  samples at the configured rate.
- An environment variable `POTHOS_AUDIO_BACKEND` selects the backend for
  the context (`null`, or a miniaudio backend name). Without it, the
  context uses the default order of miniaudio without the null backend
  (my call): a computer without a working audio system gives "no device"
  (`NotFoundException`), not a silent virtual device. An unknown name
  gives a clear exception. Document it in the README as a variable for
  tests and for problem analysis. This is my call: it adds no block
  parameter (non-goal), and the test runner sets it.

## Tests

- The method of the other toolkits:
  1. CTest test `audio.stage` installs the module into `build/stage`.
  2. CTest test `audio.selftests` runs each `/audio/tests/...` test with
     the PothosUtil of the installed core, `POTHOS_PLUGIN_PATH` to the
     stage and `POTHOS_AUDIO_BACKEND=null` (a copy of
     `cmake/PothosRunSelfTests.cmake` and `PothosTestEnv.cmake` of
     widgets).
- Self-tests in `/audio/tests`:
  - Device list: `/devices/audio/info` gives valid JSON with at least one
    device (the null device) and the miniaudio version.
  - Source: for each `dtype` and both channel modes, make the block, set
    up the default device at 44.1e3 (a rate that the null device converts
    from its internal rate), run it in a topology into a collector
    (`/blocks/collector_sink` of PothosBlocks) for a short time, and
    check that frames arrived, the port types are right, and the first
    label is `rxRate` with the value 44.1e3.
  - Sink start: a sink with a feeder that starts late gives no underflow
    report before the first data (report mode `LOGGER`, no Error record
    before the first data).
  - Sink: for each `dtype` and both channel modes, a topology
    `/blocks/feeder_source` → sink with a known number of frames; the
    sink consumes all frames within a timeout.
  - `int8` conversion: a unit test of the conversion helper
    (`-128 → 0x00`, `0 → 0x80`, `127 → 0xFF` and back).
  - Device selection: an empty name, the index `0`, the name of the null
    device, an index out of range (exception), an unknown name
    (exception).
  - `setReportMode` with an unknown mode gives an exception.
  - The overlay is valid JSON with the option "Default Device".
- The tests need PothosBlocks in the prefix of the core (feeder,
  collector). The runner checks it and stops with a clear message, as in
  comms and plotters.
- The tests must not skip a check without a message.
- The manual check of the user (decision 2): in PothosFlow, a graph audio
  source (default device) → audio sink (default device), 44.1e3 or 48e3,
  float32, 1 or 2 channels. The user hears the microphone or the line
  input on the speakers.

## Build

- `cmake_minimum_required(VERSION 3.25)`, `project(PothosAudio C CXX)` (C
  if the miniaudio file is C), C++23 (`CXX_EXTENSIONS OFF`).
- `cmake_policy(SET CMP0144 NEW)` and
  `find_package(Pothos CONFIG REQUIRED NO_PACKAGE_ROOT_PATH)`, with the
  comment of widgets that tells why.
- nlohmann json from `find_package(nlohmann_json 3.11 CONFIG REQUIRED)`
  (system packages or Conan, as in blocks). A `conanfile.txt` with
  `nlohmann_json` only (the same version as the core).
- Remove `ENABLE_AUDIO`, `cmake_dependent_option`, `FindPortaudio.cmake`,
  `find_path(json.hpp)`. The module always builds.
- `MIN_FRAMES_BLOCKING` as a compile definition of the module, as before.
- The default install prefix is the prefix of the core when the user
  gives no `CMAKE_INSTALL_PREFIX` (as in widgets).
- Options: `AUDIO_BUILD_TESTS` (default `PROJECT_IS_TOP_LEVEL`),
  `AUDIO_WARNINGS_AS_ERRORS` (default OFF).
- Delete `debian/`. Update `README.md` (dependencies: the pothos2 core,
  nlohmann json, PothosBlocks for the tests; miniaudio is in the
  toolkit; build; tests; `POTHOS_AUDIO_BACKEND`) and `Changelog.txt` (a
  "Release 0.3.0 (pending)" entry or the next version of upstream: the
  port to the pothos2 core, miniaudio in place of PortAudio).

## CI of audio

- The structure of the CI of comms (no Qt): linux-gcc (Debug, `-Werror`),
  linux-clang (Release, `-Werror`), macos (Intel runner,
  `CMAKE_OSX_DEPLOYMENT_TARGET=13.3`), windows (MSVC Release).
  1. Check out audio, `markvin/PothosCore@pothos2` and
     `markvin/PothosBlocks@pothos2`.
  2. Install the dependencies of the core, blocks and audio with Conan.
  3. Build and install the core, then blocks into the core prefix.
  4. Build audio and run `ctest -V`.
- Job `linux-system` in an `archlinux` container (`--init`): system
  packages (`asio nlohmann-json xsimd`), no Conan.
- No audio packages and no sound server in CI: the tests use the null
  backend.
- Keep the guard against downloads in the CMake files.

## PothosBase

After `pothos2` of the fork has audio:

- Add the submodule `audio` (HTTPS URL, `branch = pothos2`), the options
  `POTHOS_BASE_WITH_AUDIO` (ON by default) and `POTHOS_BASE_AUDIO_DIR`,
  and `AUDIO_WARNINGS_AS_ERRORS` from `POTHOS_BASE_WARNINGS_AS_ERRORS`.
- The audio tests need blocks: add audio to the check of the toolkits
  whose tests need blocks.
- `base.selftests` runs the audio self-tests: the test environment sets
  `POTHOS_AUDIO_BACKEND=null` (as `QT_QPA_PLATFORM=offscreen` for the Qt
  toolkits).
- `base.modules` loads the audio module.
- Configure checks: the cases without blocks turn audio off too.
- README of PothosBase: audio in the table and in the tests.

## Changes in the core

- Roadmap: phase 17 row and status.
- `CLAUDE.md`: a short note about the audio fork, its branch, its local
  clone, miniaudio in the toolkit and `POTHOS_AUDIO_BACKEND`.
- `README.md` and `Changelog.txt` of the core: PothosAudio in the list of
  ported toolkits.

## Decisions of the designer (the user must check them)

- `/devices/audio/info` keys: "Audio Device", "Backend Name", "miniaudio
  Version" in place of the PortAudio names (the structure stays).
- `POTHOS_AUDIO_BACKEND` selects the miniaudio backend (for the tests:
  `null`). No block parameter. The null backend is not in the default
  order.
- miniaudio converts the format, channels and rate; `rxRate` is the
  requested rate.
- An unknown device name (also a saved PortAudio name) is an error, no
  fallback to the default device.
- The sink reports no underflow before the first data after `activate()`.
- The ring buffer between the callback and `work()`; at an overflow the
  callback drops frames, at an underflow it outputs silence, and
  `work()` reports as before.
- `int8` goes through `ma_format_u8` with a conversion in the block.
- miniaudio is compiled with only the device I/O parts.
- nlohmann json stays a dependency from `find_package` (as in blocks).
- `debian/` is deleted.
- Order: audio → PothosBase → core docs.

## Recommended order of work

1. Fork and branch. The miniaudio copy, the new CMake, the port of
   `AudioBlock`, source, sink and info. The build is clean locally.
2. `POTHOS_AUDIO_BACKEND`, the self-tests on the null backend, the test
   runner.
3. The manual check of the user on a real device.
4. README, Changelog, delete `debian/`.
5. CI (4 jobs with Conan, and `linux-system`). PR into `pothos2` of the
   fork.
6. PothosBase: the audio submodule, the tests, CI.
7. Core docs.
