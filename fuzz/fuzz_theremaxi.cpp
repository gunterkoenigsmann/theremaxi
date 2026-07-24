// Fuzz target for the .theremaxi parser - the one place we read untrusted file
// input. The property is simple: for ANY bytes, parse_theremaxi either returns
// a Library or throws ParseError; it never crashes, reads out of bounds or
// hangs. Built two ways:
//
//   -DTHEREMINI_FUZZ=ON with clang: a libFuzzer binary for real fuzzing.
//   otherwise:                      a plain driver that replays files given as
//                                   arguments, so CI can regression-check the
//                                   corpus under the sanitizers with any
//                                   compiler.
//
// Copyright (C) 2017 Peter Niebling and contributors. GPL-3.0-or-later.

#include "library.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

static void parse_one(const uint8_t *data, size_t size)
{
	try {
		const theremaxi::Library lib = theremaxi::parse_theremaxi(
			std::string(reinterpret_cast<const char *>(data), size));
		// touch the result so nothing is optimised away
		volatile size_t n = lib.presets.size();
		(void)n;
	} catch (const theremaxi::ParseError &) {
		// the one allowed failure mode
	}
}

#ifdef THEREMINI_FUZZ

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	parse_one(data, size);
	return 0;
}

#else

#include <cstdio>
#include <fstream>
#include <sstream>

int main(int argc, char **argv)
{
	for (int i = 1; i < argc; i++) {
		std::ifstream in(argv[i], std::ios::binary);
		if (!in) {
			std::fprintf(stderr, "cannot open %s\n", argv[i]);
			return 2;
		}
		std::ostringstream ss;
		ss << in.rdbuf();
		const std::string bytes = ss.str();
		parse_one(reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size());
	}
	std::printf("replayed %d corpus file(s) without a crash\n", argc - 1);
	return 0;
}

#endif
