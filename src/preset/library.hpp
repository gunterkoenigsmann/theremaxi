// Reading and writing the .theremaxi library format - the JSON the perl app
// uses, so files are interchangeable between the two.
//
// A library is a list of presets; a preset maps a parameter id (the same ids as
// theremini_param.id: "85", "_ps", "_nr", ...) to a value that is either a
// number or a name. No wxWidgets and no hardware here, so it is tested on its
// own.
//
// Copyright (C) 2017 Peter Niebling and contributors. GPL-3.0-or-later.

#ifndef THEREMAXI_LIBRARY_HPP
#define THEREMAXI_LIBRARY_HPP

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace theremaxi {

struct Value {
	bool is_text = false;
	double number = 0.0;
	std::string text;

	static Value num(double n)
	{
		Value v;
		v.number = n;
		return v;
	}
	static Value str(std::string s)
	{
		Value v;
		v.is_text = true;
		v.text = std::move(s);
		return v;
	}

	// The perl app writes a numeric value sometimes as a JSON number and
	// sometimes as a JSON string ("12.34"), so a consumer that wants a number
	// coerces here rather than trusting the JSON type.
	double as_number() const;
};

// A preset, keyed by parameter id. std::map keeps a stable, sorted order so a
// saved file is deterministic.
using Preset = std::map<std::string, Value>;

struct Library {
	std::vector<Preset> presets;
};

// Thrown on a malformed file.
struct ParseError : std::runtime_error {
	using std::runtime_error::runtime_error;
};

Library parse_theremaxi(const std::string &json);
std::string dump_theremaxi(const Library &lib);

Library load_theremaxi(const std::string &path);
void save_theremaxi(const std::string &path, const Library &lib);

// Editing a library. Every preset carries its own position as "_nr", so these
// keep that in step - the same bookkeeping the perl librarian does.

// Set each preset's _nr to its index.
void renumber(Library &lib);

// Append a preset and return its index. Its _nr is set; if it has no _ps, one
// is added from the given name.
size_t add_preset(Library &lib, const std::string &name);

// Append a copy of an existing preset (a new _nr, the rest kept).
size_t copy_preset(Library &lib, size_t index);

// Remove a preset and renumber the rest.
void remove_preset(Library &lib, size_t index);

// The name a preset shows (its _ps), or "" if it has none.
std::string preset_name(const Preset &preset);

} // namespace theremaxi

#endif
