#!/bin/sh
# Static analysis: perlcritic on the perl reference implementation and
# clang-tidy on the C/C++. CI runs exactly this. Both tools read their settings
# from the repo root (.perlcriticrc, .clang-tidy), where the policies and checks
# that are switched off are listed together with the reason.
#
# Needs perlcritic (Ubuntu: libperl-critic-perl), clang-tidy and run-clang-tidy
# (Ubuntu: clang-tidy), and the build dependencies, because clang-tidy works
# from the compile database that cmake writes.
#
#   t/lint.sh [build-dir]     default build-dir: build-lint
set -e

cd "$(dirname "$0")/.."
root=$PWD
build=${1:-build-lint}

echo "== perlcritic =="
perlcritic --quiet --profile .perlcriticrc ThereMaxi.pl lib t tools

echo
echo "== clang-tidy =="
cmake -S . -B "$build" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON > /dev/null
# Builds first: the parameter table is generated, and the files including it
# cannot be analysed before it exists.
cmake --build "$build" > /dev/null
# Only our sources; the compile database also holds nothing else, but the
# pattern keeps generated files in the build dir out of it.
run-clang-tidy -quiet -p "$build" "$root/(src|tests|fuzz)/"
