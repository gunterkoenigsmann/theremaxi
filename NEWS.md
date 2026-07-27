# Changelog

This file is the source for the release notes: the release workflow copies the section matching
the pushed tag into the GitHub release. Versions follow [semantic versioning](https://semver.org);
the releases from 2017/2018 predate that and are listed under their original dates.

## Unreleased

Work on the C rewrite. Nothing here changes the perl application.

### Fixed

* Writing the delay time to the device put it out by about 20%: `value_export` scaled unsigned
  14-bit values to the display maximum, but the device reads them in storage units (the delay's
  wire range is ~1000 ms, wider than the 836 ms shown). It now scales by the storage divisor, which
  is the same number for every parameter except the delay time - confirmed on real hardware, where
  700 ms now writes and reads back as 700 ms. This also corrects the LV2 plugin's delay-time output.
* The default MIDI input channel for the antennas was 1, but a default-configured Theremini streams
  them on channel 0 (confirmed on the wire against firmware 1.1.1). So the antenna input, and the
  MidiFeedbackLoop feature that uses it, did not work out of the box. The defaults in `ThereMaxi.pl`
  are now channel 0; the channel is still configurable in Preferences and on the device.

### Verified

* The C read path was checked against a real Theremini (firmware 1.1.1): a 32-preset dump requested
  from the device decoded identically through the perl reference and `theremini_sysex_decode` - 864
  values, no mismatch. The factory dump is not committed, but the format lesson it taught (real
  presets are 190 packed bytes, not 174) is kept as a synthetic vector.

### Added

* `libtheremini-device` (`src/device/`): the portable core - `theremini_input_*` reassembles an
  antenna's 14-bit MIDI input from the two controllers it arrives on (or passes a 7-bit value
  straight through), and `theremini_client_matches` recognises the device by name (the reassembly
  logic moved out of `Device.pm`'s `_CC_` into a shared `lib/Input.pm`, checked against golden
  vectors) - plus the ALSA transport (`alsa.c`) that opens sequencer ports, connects to the device,
  and sends and receives MIDI. A `theremini-probe` tool exercises it: against a real Theremini it
  discovers the device, reads its identity, decodes the preset dump and shows the live antenna
  stream.
* Writing to the device: `theremini_write_preset` selects a slot, sends every value as a
  control-change (and the names as sysex), and saves - the counterpart to the read path.
  `theremini_alsa_send_cc` / `theremini_alsa_send_program` are the new transport primitives.
  `theremini-probe` gained `--set-name`, `--restore-slot` and `--backup`, used to test writing
  against the hardware. Two device findings came out of that: there are no hidden preset slots
  beyond the 32 (program change past 31 wraps into them), and the effect name is a stored string the
  documented protocol does not let us set, so a written preset keeps its delay but loses that label.
* `theremini-probe --backup FILE` saves the device's preset dump verbatim, as a restore point to
  take before anything writes to the device. The file holds the maker's factory content, so it is
  git-ignored.
* Auto-probing the MIDI channel: since the device streams its antennas continuously, the library can
  listen for a moment and report which channel they arrive on, so the channel need not be set by
  hand. `theremini_channel_probe_*` is the pure detector (tested), `theremini_alsa_detect_channel`
  drives it from the live stream, and `theremini-probe --channel` reports it - correctly detecting
  channel 0 on a default-configured device.
* The read-from-hardware path in `libtheremini-protocol`: `theremini_preset_decode` takes a preset
  apart through the sysex offset table, and `theremini_sysex_decode` / `theremini_sysex_unpack3`
  undo the device's seven-bit packing and frame a whole 32-preset dump. Checked against decoded
  dumps and unpacking groups recorded from the perl.
* The write path: `theremini_value_export` (display value to MIDI bytes, 7- and 14-bit) and the
  device control messages `theremini_msg_*` with `theremini_name_encode`. The sysex templates moved
  out of `Device.pm` into `lib/Sysex.pm` as shared data; `t/smoke.pl` pins the assembled bytes.
* An LV2 plugin (no UI) under `src/lv2/`: the Theremini's parameters as control ports, with their
  ranges, units and enum labels, emitting the matching MIDI control-change messages so a host such
  as Ardour can automate the device. The port list is generated from the parameter table. A test
  drives the plugin through its own descriptor and checks its MIDI against the protocol library; CI
  validates the generated TTL with `sord_validate`.
* The start of the wxWidgets application (`src/gui/`): a parameter editor generated from the
  protocol library. Numeric parameters are a slider paired with a spin control that stay in sync
  (drag, step or type); enums are a choice; the notebook pages and boxes come from each parameter's
  layout hints.
* `theremaxi-preset`: a small, dependency-free reader/writer for the `.theremaxi` library format, so
  files are interchangeable with the perl app. It reads the perl's mix of number- and
  string-encoded values, preserves names that need escaping, and is tested both ways - against a
  file the perl wrote and by having perl read a file it wrote.
* The GUI is now an offline librarian: open a `.theremaxi` library (menu or command line), pick a
  preset from the list to load it into the editor, Store the edited values back and Save the
  library. Loading resolves the perl's string-or-number values onto the right widgets - an enum
  index becomes the chosen scale, a string "12.34" becomes a slider position. New Library, and
  New / Copy / Delete preset, are wired to tested helpers that keep each preset's `_nr` in step
  with its position. A Preferences dialog configures the MIDI input channels (which channel and
  controller each antenna arrives on, and 7- vs 14-bit), persisted with `wxConfig`.
* Where ALSA is present, the application talks to the device: a Device menu connects (and reads the
  firmware), syncs the preset dump into the library, and auto-detects the channel, while the live
  antenna stream shows in the status bar. Run against a real Theremini, it syncs all 32 presets.
  Writing to the device is not wired up yet.
* API documentation (Doxygen) for the protocol library, checked in CI.

## 1.0.1 — 2026-07-23

### Fixed

* A filter cutoff word below 15 in an imported preset decoded to `NaN`: `**` cannot raise a
  negative number to a fractional power. `NaN` compares false against everything, so it slipped
  through the range check in `numeric::value_import`, was written into the library file — where it
  is not valid JSON — and from then on that library could not be read back at all, taking the
  program down on startup with *"malformed JSON string"*. The cube root is now taken by sign, and
  a non-finite value is clamped to the parameter's minimum as a second line of defence.

### Added

* `protocol/tables.json` and `protocol/golden.json`, generated from the tables in `lib/` by
  `tools/dump-protocol.pl`: the parameter descriptions, the sysex offsets, decoded preset dumps and
  value-to-wire sweeps. `t/check.sh` fails if they drift from the code.
* `DESIGN.md`: the plan for the split into a protocol library, a device library, a UI-less LV2
  plugin and a wxWidgets application, and how each step is tested against the perl implementation.
* The beginning of `libtheremini-protocol`: a CMake build, the parameter table generated from
  `protocol/tables.json`, and value-to-wire conversion, tested by replaying all 711 recorded
  vectors plus a monotonicity sweep over the full range of every numeric parameter.
* CI on every push and pull request, for both the perl code and the C library.
* A `.gitignore` for the files the program writes next to itself, and `.dir-locals.el` so Emacs
  keeps the tab indentation this code has always used.

### Changed

* `%CONTROLLER` and `%IMPORT` are package variables now, so the generator can read them. No
  behaviour change.

## 1.0.0 — 2026-07-23

First release of this fork, and the first version that runs on a current Linux distribution.
Tested on Ubuntu 26.10 with perl 5.40.1 and GTK 2.24.33.

### Fixed

* `File::Pid->running` calls `kill(0, undef)` when no pidfile exists yet, which is a fatal error
  since perl 5.36 (*"Can't kill a non-numeric process ID"*). The program died on every fresh start
  before showing a window. A missing pidfile now means "not running".
* `bless {}, "$base::$self"` in `lib/Controller.pm` interpolates as the variable `${base::}`
  followed by `$self`, so every controller was blessed into a package that does not exist
  (*"Can't locate object method "define" via package "\_085""*).

### Added

* `README.md` with dependency instructions for current Ubuntu, where the perl GTK2 bindings are no
  longer packaged and have to be built from CPAN, plus troubleshooting notes.
* `CLAUDE.md`, an architecture overview of the code.
* This changelog and a GitHub workflow that publishes a release when a `v*` tag is pushed.

### Known limitations

Communication with actual hardware has not been re-tested since the port — the GUI, the preset
decoder and the library round-trip have. Reports from Theremini owners are welcome.

## 2018-04-13

* Fixed comma in numeric values.
* Fixed some undefs.

## 2017-10-27

* Fixed low/high note range.
* 14-bit support for controller input.

## 2017-10-23

* Workaround for the locale problem in numeric controllers.
* Better MIDI handling.
* New feature `MidiFeedbackLoop`: use input from the antennas to manipulate preset controllers.
* Fixed import routine.

## 2017-10-19

First release of ThereMaxi by Peter Niebling.
