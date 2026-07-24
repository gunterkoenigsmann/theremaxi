// The .theremaxi reader/writer: round-trips, edge cases, and a file the perl
// app actually wrote.

#include "library.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace theremaxi;

static int failures;

static void ok(bool cond, const char *what)
{
	std::printf("%s - %s\n", cond ? "ok  " : "FAIL", what);
	if (!cond) {
		failures++;
	}
}

static bool near(double a, double b)
{
	return std::abs(a - b) < 1e-6;
}

// Read a file the perl Storage module wrote, with a mix of number and string
// values and an awkward preset name.
static void read_perl_fixture(const char *dir)
{
	const Library lib = load_theremaxi(std::string(dir) + "/perl_written.theremaxi");
	ok(lib.presets.size() == 2, "perl fixture has two presets");
	if (lib.presets.size() != 2) {
		return;
	}

	const Preset &p0 = lib.presets[0];
	ok(p0.at("_ps").text == "TEST ONE", "preset 0 name");
	ok(!p0.at("_ps").is_text ? false : true, "preset 0 name is text");
	ok(near(p0.at("74").as_number(), 55), "cc74 read as a number (55)");
	ok(p0.at("9").is_text, "cc9 was stored as a string by perl");
	ok(near(p0.at("9").as_number(), 12.34), "cc9 coerces to 12.34");
	ok(near(p0.at("29").as_number(), -800), "cc29 negative");

	const Preset &p1 = lib.presets[1];
	ok(p1.at("_ps").text == "QUOTE \"X\" AND \\ ", "preset 1 name keeps quotes and backslash");
	ok(near(p1.at("12").as_number(), 836), "cc12 max");
}

// Write, read back, compare - with names that need escaping.
static void round_trip()
{
	Library lib;
	Preset a;
	a["_ps"] = Value::str("HELLO WORLD");
	a["_nr"] = Value::num(0);
	a["85"] = Value::num(7);
	a["9"] = Value::num(12.34);
	a["29"] = Value::num(-800);
	lib.presets.push_back(a);

	Preset b;
	b["_ps"] = Value::str("weird \" \\ \n name");
	b["_nr"] = Value::num(1);
	lib.presets.push_back(b);

	const Library back = parse_theremaxi(dump_theremaxi(lib));

	ok(back.presets.size() == 2, "round-trip keeps both presets");
	ok(back.presets[0].at("_ps").text == "HELLO WORLD", "round-trip name");
	ok(near(back.presets[0].at("85").number, 7), "round-trip int value");
	ok(near(back.presets[0].at("9").number, 12.34), "round-trip float value");
	ok(near(back.presets[0].at("29").number, -800), "round-trip negative");
	ok(back.presets[1].at("_ps").text == "weird \" \\ \n name",
	   "round-trip name with quote, backslash and newline");
}

// An empty library, and the whitespace-only edges.
static void edges()
{
	ok(parse_theremaxi("[]").presets.empty(), "empty array");
	ok(parse_theremaxi("  [ ]  ").presets.empty(), "empty array with spaces");
	ok(parse_theremaxi(dump_theremaxi(Library{})).presets.empty(), "dump/parse empty");

	bool threw = false;
	try {
		parse_theremaxi("not json");
	} catch (const ParseError &) {
		threw = true;
	}
	ok(threw, "garbage input throws ParseError");
}

// The output must be valid JSON that the perl app can read back. Shell out to
// perl to prove it, when perl is available.
static void perl_reads_our_output(const char *dir)
{
	Library lib;
	Preset p;
	p["_ps"] = Value::str("BACK \" TO PERL");
	p["_nr"] = Value::num(0);
	p["12"] = Value::num(836);
	lib.presets.push_back(p);

	const std::string path = std::string(dir) + "/cpp_written.theremaxi";
	save_theremaxi(path, lib);

	const std::string cmd =
		"perl -e 'require q(./lib/Storage.pm); "
		"my @d = ThereMaxi::Storage->load(q(" + path + ")); "
		"exit(($d[0]->{_ps} eq q(BACK \" TO PERL) and $d[0]->{12}==836) ? 0 : 1)'";
	const int rc = std::system(cmd.c_str());
	if (rc < 0) {
		std::printf("ok   - (perl not run; skipped write-compat check)\n");
	} else {
		ok(rc == 0, "perl reads back what we wrote");
	}
}

int main(int argc, char **argv)
{
	const char *dir = argc > 1 ? argv[1] : ".";
	read_perl_fixture(dir);
	round_trip();
	edges();
	perl_reads_our_output(argc > 2 ? argv[2] : ".");

	if (failures) {
		std::printf("\n%d check(s) failed\n", failures);
		return 1;
	}
	std::printf("\nall checks passed\n");
	return 0;
}
