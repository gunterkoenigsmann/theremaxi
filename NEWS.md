# Changelog

This file is the source for the release notes: the release workflow copies the section matching
the pushed tag into the GitHub release. Versions follow [semantic versioning](https://semver.org);
the releases from 2017/2018 predate that and are listed under their original dates.

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
