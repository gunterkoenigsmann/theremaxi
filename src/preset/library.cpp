#include "library.hpp"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace theremaxi {

double Value::as_number() const
{
	if (!is_text) {
		return number;
	}
	// the app also stored numbers under non-C locales with a comma; accept it
	std::string t = text;
	for (char &c : t) {
		if (c == ',') {
			c = '.';
		}
	}
	return std::strtod(t.c_str(), nullptr);
}

namespace {

// A small parser for exactly what .theremaxi contains: an array of objects
// whose values are strings or numbers. Not a general JSON library.
class Parser {
public:
	explicit Parser(const std::string &text) : m_p(text.c_str()), m_end(m_p + text.size()) {}

	Library parse()
	{
		Library lib;
		skip_ws();
		expect('[');
		skip_ws();
		if (peek() == ']') {
			++m_p;
			return lib;
		}
		for (;;) {
			lib.presets.push_back(parse_object());
			skip_ws();
			const char c = next();
			if (c == ']') {
				break;
			}
			if (c != ',') {
				fail("expected ',' or ']' between presets");
			}
			skip_ws();
		}
		return lib;
	}

private:
	const char *m_p;
	const char *m_end;

	[[noreturn]] void fail(const std::string &why) const
	{
		throw ParseError("theremaxi: " + why);
	}

	char peek() const
	{
		if (m_p >= m_end) {
			fail("unexpected end of input");
		}
		return *m_p;
	}
	char next()
	{
		const char c = peek();
		++m_p;
		return c;
	}
	void expect(char c)
	{
		if (next() != c) {
			fail(std::string("expected '") + c + "'");
		}
	}
	void skip_ws()
	{
		while (m_p < m_end && (*m_p == ' ' || *m_p == '\t' || *m_p == '\n' || *m_p == '\r')) {
			++m_p;
		}
	}

	Preset parse_object()
	{
		Preset preset;
		skip_ws();
		expect('{');
		skip_ws();
		if (peek() == '}') {
			++m_p;
			return preset;
		}
		for (;;) {
			skip_ws();
			const std::string key = parse_string();
			skip_ws();
			expect(':');
			skip_ws();
			preset[key] = parse_value();
			skip_ws();
			const char c = next();
			if (c == '}') {
				break;
			}
			if (c != ',') {
				fail("expected ',' or '}' in preset");
			}
		}
		return preset;
	}

	Value parse_value()
	{
		const char c = peek();
		if (c == '"') {
			return Value::str(parse_string());
		}
		if (c == 'n') { // null: treat as an absent value, stored as 0
			expect_word("null");
			return Value::num(0);
		}
		return Value::num(parse_number());
	}

	void expect_word(const char *word)
	{
		for (const char *w = word; *w; ++w) {
			if (next() != *w) {
				fail(std::string("expected '") + word + "'");
			}
		}
	}

	std::string parse_string()
	{
		expect('"');
		std::string out;
		for (;;) {
			char c = next();
			if (c == '"') {
				break;
			}
			if (c == '\\') {
				const char e = next();
				switch (e) {
				case '"': out += '"'; break;
				case '\\': out += '\\'; break;
				case '/': out += '/'; break;
				case 'b': out += '\b'; break;
				case 'f': out += '\f'; break;
				case 'n': out += '\n'; break;
				case 'r': out += '\r'; break;
				case 't': out += '\t'; break;
				case 'u': out += parse_unicode(); break;
				default: fail("bad string escape");
				}
			} else {
				out += c; // raw UTF-8 bytes pass through, as JSON::PP writes them
			}
		}
		return out;
	}

	// \uXXXX -> UTF-8. Only the basic plane; surrogate pairs are not expected
	// in a preset name but are handled so a stray one does not corrupt output.
	std::string parse_unicode()
	{
		unsigned code = 0;
		for (int i = 0; i < 4; ++i) {
			const char c = next();
			code <<= 4;
			if (c >= '0' && c <= '9') {
				code |= static_cast<unsigned>(c - '0');
			} else if (c >= 'a' && c <= 'f') {
				code |= static_cast<unsigned>(c - 'a' + 10);
			} else if (c >= 'A' && c <= 'F') {
				code |= static_cast<unsigned>(c - 'A' + 10);
			} else {
				fail("bad \\u escape");
			}
		}
		std::string out;
		if (code < 0x80) {
			out += static_cast<char>(code);
		} else if (code < 0x800) {
			out += static_cast<char>(0xc0 | (code >> 6));
			out += static_cast<char>(0x80 | (code & 0x3f));
		} else {
			out += static_cast<char>(0xe0 | (code >> 12));
			out += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
			out += static_cast<char>(0x80 | (code & 0x3f));
		}
		return out;
	}

	double parse_number()
	{
		const char *start = m_p;
		while (m_p < m_end) {
			const char c = *m_p;
			if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' ||
			    c == 'e' || c == 'E') {
				++m_p;
			} else {
				break;
			}
		}
		if (m_p == start) {
			fail("expected a number");
		}
		return std::strtod(std::string(start, m_p).c_str(), nullptr);
	}
};

void write_string(std::ostream &os, const std::string &s)
{
	os << '"';
	for (char c : s) {
		switch (c) {
		case '"': os << "\\\""; break;
		case '\\': os << "\\\\"; break;
		case '\b': os << "\\b"; break;
		case '\f': os << "\\f"; break;
		case '\n': os << "\\n"; break;
		case '\r': os << "\\r"; break;
		case '\t': os << "\\t"; break;
		default:
			if (static_cast<unsigned char>(c) < 0x20) {
				char buf[8];
				std::snprintf(buf, sizeof buf, "\\u%04x", c);
				os << buf;
			} else {
				os << c; // raw UTF-8, matching JSON::PP
			}
		}
	}
	os << '"';
}

void write_number(std::ostream &os, double n)
{
	// integers without a trailing .0, the way JSON::PP renders them
	if (std::floor(n) == n && std::abs(n) < 1e15) {
		os << static_cast<long long>(n);
	} else {
		char buf[32];
		std::snprintf(buf, sizeof buf, "%.10g", n);
		os << buf;
	}
}

} // namespace

Library parse_theremaxi(const std::string &json)
{
	return Parser(json).parse();
}

std::string dump_theremaxi(const Library &lib)
{
	std::ostringstream os;
	os << "[\n";
	for (size_t i = 0; i < lib.presets.size(); ++i) {
		os << "   {\n";
		const Preset &preset = lib.presets[i];
		size_t j = 0;
		for (const auto &[key, value] : preset) {
			os << "      ";
			write_string(os, key);
			os << " : ";
			if (value.is_text) {
				write_string(os, value.text);
			} else {
				write_number(os, value.number);
			}
			os << (++j < preset.size() ? ",\n" : "\n");
		}
		os << "   }" << (i + 1 < lib.presets.size() ? ",\n" : "\n");
	}
	os << "]\n";
	return os.str();
}

Library load_theremaxi(const std::string &path)
{
	std::ifstream in(path, std::ios::binary);
	if (!in) {
		throw ParseError("cannot open " + path);
	}
	std::ostringstream ss;
	ss << in.rdbuf();
	return parse_theremaxi(ss.str());
}

void save_theremaxi(const std::string &path, const Library &lib)
{
	std::ofstream out(path, std::ios::binary);
	if (!out) {
		throw ParseError("cannot write " + path);
	}
	out << dump_theremaxi(lib);
}

void renumber(Library &lib)
{
	for (size_t i = 0; i < lib.presets.size(); ++i) {
		lib.presets[i]["_nr"] = Value::num(static_cast<double>(i));
	}
}

size_t add_preset(Library &lib, const std::string &name)
{
	Preset preset;
	preset["_ps"] = Value::str(name);
	lib.presets.push_back(std::move(preset));
	renumber(lib);
	return lib.presets.size() - 1;
}

size_t copy_preset(Library &lib, size_t index)
{
	if (index >= lib.presets.size()) {
		throw ParseError("copy_preset: index out of range");
	}
	lib.presets.push_back(lib.presets[index]);
	renumber(lib);
	return lib.presets.size() - 1;
}

void remove_preset(Library &lib, size_t index)
{
	if (index >= lib.presets.size()) {
		throw ParseError("remove_preset: index out of range");
	}
	lib.presets.erase(lib.presets.begin() + static_cast<std::ptrdiff_t>(index));
	renumber(lib);
}

std::string preset_name(const Preset &preset)
{
	const auto it = preset.find("_ps");
	return it != preset.end() ? it->second.text : std::string();
}

} // namespace theremaxi
