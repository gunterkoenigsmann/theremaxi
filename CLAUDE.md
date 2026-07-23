# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

ThereMaxi is a Linux GTK2 editor/librarian for the Moog Theremini, written entirely in Perl 5 (originally developed against 5.22). It talks to the device over ALSA MIDI: CC messages for live parameter tweaking, sysex for bulk preset dumps. GPL-3.0.

## Running / checking

There is no build system, no test suite, and no lint config. The program is run directly:

```sh
./ThereMaxi.pl                       # needs /usr/bin/perl; edit the shebang otherwise
./ThereMaxi.pl --cleanstate          # ignore the saved state file on startup
./ThereMaxi.pl --statefile=/path --pidfile=/path
```

Runtime deps are checked in a `BEGIN` block in `ThereMaxi.pl` and the program dies listing whatever is missing: `File::Pid FindBin Getopt::Long Gtk2 JSON::PP MIDI::ALSA MIME::Base64 sigtrap`. `README.md` has the full Ubuntu install recipe — the notable part is that `libgtk2-perl` is no longer packaged and must be built from CPAN with `-Wno-incompatible-pointer-types -Wno-implicit-function-declaration`.

Verified working on Ubuntu 26.10 / perl 5.40.1 / GTK 2.24.33. If the modules are installed under a prefix rather than system-wide, `PERL5LIB` must point at it.

**Per-file `perl -c` does not work on most modules** and is not a useful check here: the `Controller` subclasses `use base` on packages that live in files `@INC` cannot map to (`ThereMaxi::Controller::numeric` is `lib/Controller/numeric.pm`), so they die with *"Base class package … is empty"*. Only files whose base classes are already loaded, or which have no `use base`, pass standalone.

Run `t/check.sh` instead — it is everything that works without a Theremini, a display or non-core modules, and it is exactly what CI runs on every push and before publishing a tag:

* `t/smoke.pl` loads the whole non-GUI tree in order and exercises decode, export and the library round-trip.
* `perl -c ThereMaxi.pl` against throwaway stubs for the three non-core modules its `BEGIN` block requires.
* `perl -c` on the handful of modules that do stand alone.

Add new non-GUI behaviour to `t/smoke.pl`; it is the only thing that compiles the `Controller` subclasses.

Only one instance may run at a time (pidfile in `$XDG_RUNTIME_DIR`, else the source dir).

`t/smoke.pl` shows how to drive the code without a Theremini: set up the `$main::` globals the way `ThereMaxi.pl` does, stub `ThereMaxi::Device::init`/`sync_on_change`/`value_changed` with no-ops, then call `Preset->init`. Note `Preset::init` is what fills Preset's own `%CONTROLLER`; without it `import_data` silently returns nothing. Extend that file when adding non-GUI behaviour.

**Modern-perl traps** (both already fixed, but the same patterns may lurk elsewhere): `kill(0, undef)` is fatal since 5.36 — this broke `File::Pid->running` on a missing pidfile; and `"$pkg::$name"` interpolates as `${pkg::}` — this broke the `bless` in `Controller::new`. When touching old code here, run it, don't just compile it.

## Loading scheme and globals

Modules are **not** loaded via `@INC`/`use lib`. Every file is pulled in with `require "$main::LIB/Some/File.pm"` from its parent, so a new module only exists once an existing module `require`s it explicitly. The load chain is `ThereMaxi.pl` → `Editor.pm` → `Event/Controller/Preset/Library/Device/gtk/Layout` → `Toolbar/Presets/Library/Feature` → …

Package names use `::` (`ThereMaxi::Editor::Prefs::General`) but file paths use `_` for the deeper level (`lib/Editor/Prefs_General.pm`). Don't assume path == package.

Globals set by `ThereMaxi.pl` and used everywhere:

- `$main::NAME` — "ThereMaxi", also the ALSA client name and window title
- `$main::CWD` / `$main::LIB` — source dir and `lib/`
- `%main::STATE` — the entire persistent config/session state

`%STATE` is loaded from JSON at startup (`ThereMaxi.state`) and written back on clean exit; defaults for missing keys are filled in at the top of `ThereMaxi.pl`. Widgets read and write `$main::STATE{...}` directly (window geometry, pane positions, notebook tab, `{preset}` = currently selected `"library/nr"`, per-controller prefs, MidiFeedbackLoop rows). Anything that should survive a restart goes there; nothing else persists it.

## Event bus

`lib/Event.pm` is a tiny pub/sub with a **hard-coded whitelist** of event names in `%EVENTS`. `connect`/`fire` die on an unknown name, so adding an event means adding it to that list first. `catch`/`release` queue events during bulk operations (device sync, import) and flush them afterwards.

Almost all GUI enable/disable logic is expressed as `Event->connect` handlers rather than direct calls between widgets — e.g. toolbar buttons subscribe to `device/discover`, `device/sync_on_change`, `switch/library`, `switch/presets`, `preset/changed`. When adding UI that reacts to device or selection changes, follow that pattern instead of poking widgets from `Device.pm`.

Handlers receive the event name as the first argument, hence the pervasive `sub{ ... pop }` idiom to grab the last value.

## Controllers

`lib/Controller.pm` holds `%CONTROLLER`, the master table of every editable parameter, keyed by **MIDI CC number** (7, 9, 12, …) or a **pseudo-CC** string (`_nr`, `_ps`, `_fx`). Each row carries `name`, `show => [sort, tab, frame, label]` (used for the prefs listing and sorting), `preset` (belongs to a preset vs. global), `typ`, numeric `min/max/dig/fmt`, and `props` (`sync_on_change`, `MidiFeedbackLoop`).

`Controller->new($CC)` is a **singleton factory with filename-based dispatch**: it looks for `lib/Controller/_%03d.pm` (or `<pseudo-CC>.pm`), falls back to the `typ` field, `require`s that file and blesses into `ThereMaxi::Controller::<name>`. So CC 85 → `lib/Controller/_085.pm`; CC 71 has no file → `typ=>'prozent'` → `lib/Controller/prozent.pm`. Repeat calls return the cached `__SELF__`.

Class hierarchy:

```
Controller ─┬─ numeric ─┬─ prozent ─── _074
            │           └─ _090dep            (sensitivity depends on waveform)
            ├─ choice ──┬─ _080 _085 _090     (range() returns the combo entries)
            │           └─ _086 ─── _notes
            └─ name ────┬─ _ps                (preset name entry + rename button)
                        └─ _fx                (delay presets that drive CC 12/14)
```

Subclasses override `range()` (choice lists), `widget()` (the Gtk widget + a setter closure passed to `SUPER::widget`), `value_import`/`value_export`/`value_compare`, and optionally `depend()` — if a class defines `depend`, the base class auto-subscribes it to `controller/set_value` and `controller/value_changed` so parameters can grey each other out.

**Value units:** the GUI works in display units (Hz, %, ms, semitones); `value_export` converts to MIDI wire format, returning either a 7-bit int or `[msb, lsb]` for 14-bit CCs (`$CC < 32 && range > 0x7f`), which `Device::midi_cc` then sends as CC and CC+32. `value_import` converts the other way from sysex.

Adding a parameter = add a row to `%CONTROLLER`, optionally add a subclass file, and place it in a tab in `lib/Editor/Layout.pm` (the `show` field does *not* place it — `_Basic`/`_Advanced`/`_Global` list CC numbers explicitly).

## Device / MIDI

`lib/Device.pm` opens an ALSA client with 3 ports: port 0 subscribed to `System:1` for announcements (device hotplug discovery), port 1 = input from the Theremini, port 2 = output to it. Discovery is a scan of `MIDI::ALSA::listclients` for a client matching `/theremini/i`, triggered by ALSA announce events, not a timer, despite the name `discovery_interval` (which is really a boolean toggle).

Incoming CC traffic is dispatched through `Glib::IO->add_watch` on the ALSA fd to `_CC_`, which maps the configured volume/pitch antenna channel+CC (`$STATE{device}{midi_input}`) to registered callbacks — this is how antenna movement feeds features. 14-bit input is reassembled from CC and CC+32.

Sysex is sent via `midi_sx(qw( hex bytes ))` and read back by `midi_read`, which **blocks with an `alarm()` timeout** (`midi_read_timeout`). Preset dump decoding lives in `Preset::sysex` + `Preset::_sx_` (7-bit unpacking) and the `%IMPORT` byte-offset table in `lib/Preset.pm`, which maps sysex offsets → CC, pack template, and a scaling divisor. Several offsets are still marked unknown.

Two device modes matter throughout the UI: **sync-on-change** (every edit is pushed to the device immediately, only for controllers with the `sync_on_change` prop; their labels turn orange) and manual Send/QuickSave. Bulk operations wrap themselves in `sync_on_change(0)` + `Event->catch` and restore afterwards.

**Error convention:** the top-level `$SIG{__DIE__}` in `ThereMaxi.pl` removes the pidfile and exits, so any recoverable operation must run inside `eval { local $SIG{__DIE__}; ... }` and pass `$@` to `Device::error`, which fires the `ERROR` event (a modal dialog installed in `Editor::init`). Forgetting the `local` turns a MIDI hiccup into a hard exit.

## Storage: libraries and presets

`lib/Library.pm` keeps `%LIBRARY`, an in-memory hash of `name => [ preset-hashes ]`, persisted by `lib/Storage.pm` as pretty JSON:

- `<library_path>/<name>.theremaxi` — user libraries
- `<library_path>/.theremaxi` — the presets read off the device, stored under the library key `'.'` (shown as "Theremini Presets"; `drop_preset` refuses to touch it)

`library_path` defaults to `<source dir>/data` and is configurable in Preferences. A preset is a flat hash of `CC => value` plus `_nr` and `_ps`. `$STATE{preset}` is the `"library/nr"` path of the current selection; `$STATE{online}` mirrors the preset slot selected on the device.

`import_xml` reads Moog's `.theremini` files by grepping `data="..."` attributes, base64-decoding, and feeding them through the same `Preset::import_data` used for sysex.

## GUI conventions

- `lib/Editor/gtk.pm` monkey-patches convenience constructors into the Gtk2 namespaces (`Gtk2::Frame->new_with_object`, `Gtk2::Label->new_with_markup`, `Gtk2::ScrolledWindow->new_with_viewport`, …). These are project-local, not Gtk2 API — that file must be loaded before any layout code.
- `lib/Editor/Layout.pm` builds the window: toolbar / HPaned(library+presets | notebook) / statusbar. Notebook tabs come from `_Basic`, `_Advanced`, `_Global`, then one tab per entry in `Feature->list`.
- Features are the extension point: `lib/Feature.pm` lists plugin objects, each returning a widget + tab label. `Feature/MidiFeedbackLoop.pm` is the only one — it maps antenna input onto arbitrary controllers with min/max/sensitivity/invert per row, opting in via the `MidiFeedbackLoop` prop.
- Label/widget alignment across frames is done with shared `Gtk2::SizeGroup`s threaded through the `LAYOUT` key of each controller.

## Style

Existing code uses tabs, Allman braces, no trailing-comment noise, `&subname` calls with implicit `@_`, `sub foo { my(undef,$x) = @_ }` for class-method calls, and one-line closures for signal handlers. Occasional comments are in German. Match it rather than modernising.

Watch for the locale workaround `$value =~ tr/,/./` in numeric code — the original author hit comma decimal separators from Gtk under non-C locales; several past bugfixes were exactly this.
