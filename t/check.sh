#!/bin/sh
# Everything that can be checked without a Theremini, a display or non-core
# perl modules. Run it from anywhere; CI runs exactly this.
set -e

cd "$(dirname "$0")/.."

echo "== smoke test =="
perl t/smoke.pl

echo
echo "== syntax check =="
# The dependency check in the BEGIN block of ThereMaxi.pl runs during -c, so
# stub the three non-core modules it requires. This checks syntax only, the
# stubs are never called into.
stubs=$(mktemp -d)
trap 'rm -rf "$stubs"' EXIT
mkdir -p "$stubs/MIDI" "$stubs/File"
printf 'package Gtk2;\nsub import {}\n1;\n' > "$stubs/Gtk2.pm"
printf 'package MIDI::ALSA;\n1;\n'          > "$stubs/MIDI/ALSA.pm"
printf 'package File::Pid;\n1;\n'           > "$stubs/File/Pid.pm"
PERL5LIB="$stubs" perl -c ThereMaxi.pl

# Modules with no use base, or whose base class is core, stand on their own.
# The rest are compiled by the smoke test, which loads the tree in order.
for f in lib/Storage.pm lib/Event.pm lib/Controller.pm lib/Preset.pm \
         lib/Library.pm lib/Editor.pm lib/Editor/Layout.pm; do
	perl -c "$f"
done
