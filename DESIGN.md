# Where this is going

The perl program works, but it is one monolith: protocol knowledge, ALSA I/O and GTK2 widgets in
the same files. The target is four pieces with one shared core.

## Status

* **`libtheremini-protocol`** — the byte-level language of the device is done and tested against the
  perl: the parameter table, value ↔ wire in both directions, preset dump → presets (including the
  seven-bit unpacking and framing), and the device control messages. See the C under
  `src/protocol/` and the tests under `tests/`.
* **LV2 plugin (no UI)** — done: `src/lv2/`, ports generated from the parameter table, driven by a
  descriptor-level test and validated with `sord_validate`.
* **`libtheremini-device`** — not started. The ALSA transport; see below. This is the part that
  needs hardware to verify, so its pure pieces (discovery matching, 14-bit input reassembly) will be
  split out and tested, and the ALSA I/O kept behind a backend seam.
* **wxWidgets application** — started (`src/gui/`). The parameter editor is built from the protocol
  library: a notebook whose pages and boxes come from each parameter's layout hints, numeric
  parameters as a slider paired with a `wxSpinCtrlDouble`, enums as a choice. No device or library
  management yet. Builds where wxWidgets is present.

```
                       protocol/tables.json          (generated from lib/, authoritative)
                                 |
                                 v
                    libtheremini-protocol            no I/O, no threads, no allocation
                    presets <-> bytes, values <-> wire
                       /                 \
                      /                   \
      libtheremini-device            LV2 plugin (no UI)
      ALSA sequencer, discovery,     control ports -> MIDI CC out
      sysex transfer                 hosted by Ardour et al
                      \
                       \
                     wxWidgets application
                     editor and librarian
```

## The two libraries

**`libtheremini-protocol`** is the valuable part and the one that must outlive everything else. It
knows what parameters exist, how a sysex preset dump is laid out, and how a display value (Hz, %,
ms, semitones) maps to the bytes on the wire. It performs no I/O, spawns no threads, and does not
allocate in any function the LV2 `run()` callback can reach — that last constraint is what lets the
plugin and the application share it.

Its tables are not hand-written. `protocol/tables.json` is generated from the perl source by
`tools/dump-protocol.pl` and the C tables are generated from that JSON at build time. Transcription
is the main risk in this port — the CC table has 30 entries and the sysex offset table 28, all of
them plausible-looking numbers whose errors are silent — so nobody gets to retype them.

**`libtheremini-device`** owns the ALSA sequencer client, device discovery, the sysex request and
response cycle, and the "sync on change" mode. It depends on the protocol library, never the other
way round.

Not yet extracted: the sysex message templates still live as literals in `lib/Device.pm`
(`04 0B 06 03 …` requests all presets, `07`/`08` write the preset and effect names, `7E 7F 06 01`
is the identity request, CC 119 saves to the current preset). These must move into the generated
tables before the device library is written, by the same rule as above.

## LV2 plugin

No UI, and built. A plugin that declares its control ports with proper ranges and `units:unit` gets
a host-generated interface for free, and for a parameter editor that is most of what is wanted at
zero toolkit risk. It also sidesteps a concrete problem: Ardour is still GTK2 (it ships
`libsuil_x11_in_gtk2.so`), wxGTK is GTK3, and both in one process is an immediate abort. Keeping wx
out of the plugin entirely is not a compromise here, it is the design.

The plugin (`src/lv2/theremini.c`) exposes each CC parameter as a control port and emits the
matching control-change messages, on channel 0, whenever a port changes; `run()` allocates nothing.
`tools/generate-lv2.pl` writes both the port description (`theremini.ttl`) and the C port table
(`lv2_ports.h`) from `protocol/tables.json`, so the two cannot drift from each other or from the
parameters. `tests/test_lv2.c` instantiates the plugin through its descriptor, runs it and checks
the emitted MIDI against `theremini_value_export`; CI validates the TTL with `sord_validate`.

Not yet exposed as ports: the two names (they are sysex, awkward as control ports) and preset
selection (a program change). Those belong to the device library or a later plugin revision.

## wxWidgets application

The standalone editor and librarian: the tree of libraries and presets, the tabs, the
MidiFeedbackLoop feature. wxWidgets is chosen for its stable source API — note that on Linux it is
implemented on GTK3, so this buys insulation from toolkit churn, not independence from GTK.

The parameter editor exists (`src/gui/`). Each numeric parameter is a slider paired with a
`wxSpinCtrlDouble` that stay in sync, so a value can be dragged, stepped or typed — deliberately
not the reference editor's min/mid/max labels under each slider, which waste vertical space. The
notebook pages, the boxes within them and the control order all come from the layout hints on
`theremini_param` (`tab`, `group`, `label`, `order`), so the editor is generated from the same
table as everything else.

It is also a working offline librarian: open a `.theremaxi` library, and its presets fill a list;
selecting one loads its values into the editor; Store writes the editor back to the preset; Save
writes the library. The file format is handled by `theremaxi-preset` (`src/preset/`), a
dependency-free reader/writer tested against files the perl actually wrote and by having perl read
files it writes. Still to come: values to and from the device and the LV2 plugin, creating and
deleting libraries and presets, and the MidiFeedbackLoop tab.

## Testing

The perl implementation is the oracle. It works, it is the only description of the protocol that
has ever talked to real hardware, and it stays runnable until something else reproduces its output
exactly.

**Golden vectors** — `protocol/golden.json`, generated, committed, and checked for drift by
`t/check.sh` on every push. It contains eight decoded preset dumps (all zeroes, all 0xff, and six
from a seeded PRNG so they are reproducible across perl versions) and, for all 24 tunable
parameters, a sweep across the full range with the exact bytes the perl code puts on the wire. A C
implementation is correct when it reproduces this file.

**Exhaustive rather than sampled** — each parameter's wire domain is 128 values, or 16384 for the
14-bit ones. The whole input space is a few hundred thousand cases and runs in under a second, so
test all of it: `import(export(x)) == x` within tolerance, and `export` monotonic.

**Differential** — bind the C library into perl with `FFI::Platypus` (no XS glue) and run both
implementations over the same cases, asserting equality. This catches the cases the golden file
does not enumerate.

**Fuzzing** — the sysex parser is the one component that eats untrusted input, and in C that is
where buffer overruns live. Seed afl or libFuzzer with the golden corpus; assert no crash, and
identical output wherever perl accepts the input.

**Byte-exact round-trip** — a property the perl code does not have and the C library must. The
offset table still has unknown regions (`0x1c..0x1f`, `0x28..0x29`, `0x46..0x4f`, `0x74..EOF`).
Perl only ever reads sysex and writes changes back as CC, so it never had to preserve them. A
library that serializes presets must round-trip unknown bytes verbatim: `parse → serialize → memcmp`.

**Plugin conformance** — `lv2lint` and `sord_validate` on the TTL, in CI.

**Real-time safety** — assert that `run()` does not allocate, either under LLVM's RealtimeSanitizer
or with a malloc hook that aborts while the callback is on the stack.

**Hardware, last and by capture** — record the perl app performing a full preset send with
`aseqdump`, replay the same operation through the C stack, diff the byte streams. No unit test
reaches the device layer; a captured stream does.

**Range discovery, with hardware** — the parameter ranges are faithful to the original, but the
original only set minimums explicitly for the parameters that go negative; the rest inherited 0 as a
default, so a floor of 0 is unverified for a few (Wavetable Scan Rate in Hz; Filter Cutoff, which
hides a frequency behind a 0–100% curve). The device can be asked directly: write a sweep of values
to a parameter as CC, read the preset dump back, and decode it with `theremini_sysex_decode` — the
decoder is already built. Intended vs. stored reveals the effective range, any dead zone (e.g. "0 =
off, then 5–100%") and the quantization step. Any correction goes into `min`/`max` in
`lib/Controller.pm` and propagates through the generated table.

## Decisions already forced by the data

Generating the golden vectors turned up a bug before any C existed: a filter cutoff word below 15
decoded to `NaN`, which no comparison rejects, so it reached the library file as invalid JSON and
made that library permanently unloadable. Fixed in 1.0.1, with a regression test in `t/smoke.pl`
and a guard in the generator that refuses to emit a non-finite number. Three further things a
reimplementation has to take a position on:

**`value_export` collapses at the ends of the range.** For a 14-bit parameter the sweep gives
`{value: 810, wire: [124, 1]}` but `{value: 836, wire: [127]}` — at exactly the maximum,
`lib/Controller/numeric.pm` returns a scalar `0x7f` instead of a pair, so only the MSB controller is
sent and the device keeps whatever LSB it had. The same happens at the minimum. Whether to
reproduce this or fix it cannot be decided without hardware.

**Enum values from a dump are not validated.** The all-0xff case decodes to `Scale = 65535` with 22
scales defined. In perl that is a harmless out-of-range `set_active`; in C it is an array index.
The decoder must clamp or reject.

**Locale.** Several places do `tr/,/./` because the perl code formats through `sprintf` and reads
back. A C library should never round-trip numbers through strings, which removes the whole class —
but the golden vectors were generated by code that does, so a mismatch in the last digit is
expected there and should be compared with a tolerance, not with `==`.

## Licensing

The perl code is GPL-3.0 by Peter Niebling. Anything derived from it — and a protocol library
generated from its tables certainly is — inherits that. "Reusable library" therefore means reusable
by GPL-compatible software only. If wider reuse is wanted (the usual convention for LV2 plugins is
ISC or MIT), that needs the original author's agreement. Worth settling before the code exists
rather than after.

## Build

CMake, decided. wxWidgets ships CMake config files, LV2 plugins build fine with it, and CPack
covers most of the packaging from the same tree.

What CPack does and does not do is worth knowing before the packaging work starts: it generates
`.deb`, `.rpm`, `.dmg` (DragNDrop), Windows installers (NSIS or WiX) and plain archives. It does
*not* generate flatpaks or snaps — those need their own `flatpak-builder` manifest and
`snapcraft.yaml`, both driven by the same CMake install rules. So one set of `install()` commands
feeds every format; only the two sandboxed ones need a hand-written file each.

## Platforms

Portability is a property of one component. The protocol library is pure C with no I/O and is
already portable; wxWidgets and LV2 are portable by construction. Only `libtheremini-device` is
Linux-bound, because it speaks to the ALSA sequencer directly.

So the whole cost of macOS and Windows support is one backend abstraction in that library: ALSA on
Linux, CoreMIDI on macOS, WinMM or WinRT on Windows — or a single dependency such as libremidi,
RtMidi or PortMidi that covers all three. Worth designing the interface for that from the start
even if only ALSA is ever implemented, because retrofitting a backend seam is the expensive kind of
refactor. Device discovery is the part that will not abstract cleanly: matching a client name
against `/theremini/i` is an ALSA-shaped idea.

GitHub runners cover all three platforms, so the same checks can run on a matrix once the code is
C rather than perl.

## Packaging

Attach packages to every release. Note that this targets the rewrite, not the perl program — a
Debian package of the perl version would depend on `libgtk2-perl`, which no longer exists.

**Debian package** is the natural home for all of it: library, application, and the plugin in
`/usr/lib/lv2/`. Everything is visible to every host on the system, no sandbox questions. This
should be the primary artifact.

**Flatpak** is the one that can do plugins properly, contrary to expectation. Flathub defines
extension points for audio plugins — `org.freedesktop.LinuxAudio.Plugins.*` — and hosts packaged as
flatpaks (Ardour among them) pick up plugins installed as such extensions. So the flatpak build
should produce two things: the standalone application, and the plugin as a LinuxAudio extension.

**Snap** cannot do this. Strict confinement is app-centric, and there is no mechanism for one snap
to expose an LV2 plugin to a host in another. Ship the standalone application as a snap and accept
that the plugin is not visible from it — as you say, the packages that cannot announce the plugin
still have value for the application.

All three build in CI. The release workflow already publishes on a `v*` tag, so adding jobs that
build the artifacts and attach them with `gh release upload` is a small extension of what exists.
