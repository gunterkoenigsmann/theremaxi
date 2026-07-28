/* theremini-ctl - control a Moog Theremini from the command line.
 *
 * Everything the GUI does to the device is available here without a display, so
 * a Theremini can be driven from a script or over SSH: identify it, detect the
 * antenna channel, back up and restore its presets, rename a slot, push a single
 * parameter live, send a preset out of a .theremaxi library into a slot, or pull
 * the whole device into a library.
 *
 * The device operations live in the C libraries (theremini-device,
 * theremini-protocol); reading and writing .theremaxi libraries lives in the C++
 * preset library. This tool is only the argument grammar over the two. The
 * grammar is documented in theremini-ctl(1).
 *
 * Copyright (C) 2017 Peter Niebling and contributors. GPL-3.0-or-later.
 */

#include "library.hpp"

#include "theremini/alsa.h"
#include "theremini/device.h"
#include "theremini/protocol.h"
#include "theremini/write.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

const char *const kUsage =
	"Usage: theremini-ctl [--channel N] <command> [arguments]\n"
	"\n"
	"Device commands (need a connected Theremini):\n"
	"  identify                     connect and report the firmware version (default)\n"
	"  detect                       report the MIDI channel the antennas stream on\n"
	"  dump                         fetch the preset dump and report the count\n"
	"  backup <file.syx>            save the raw preset dump as a restore point\n"
	"  restore <slot> <file.syx>    write one slot from a backup back to the device\n"
	"  set-name <slot> <name>       rename a slot's preset and save it\n"
	"  set <cc> <value>             send one parameter live (value in display units)\n"
	"  send-preset <lib> <n> [--slot <slot>]\n"
	"                               send preset n of a .theremaxi library to a slot\n"
	"  sync <lib.theremaxi>         read the device's presets into a library file\n"
	"\n"
	"Offline commands (no device):\n"
	"  params                       list every parameter, its CC and range\n"
	"  help                         show this message\n"
	"\n"
	"Slots and preset numbers are 1-based, as shown on the device (1-32).\n"
	"--channel sets the MIDI channel used by set/send-preset/restore/set-name\n"
	"(default 0, a factory-configured device).\n";

int usage_error(const char *message)
{
	if (message) {
		fprintf(stderr, "theremini-ctl: %s\n", message);
	}
	fputs(kUsage, stderr);
	return 2;
}

/* --------------------------------------------------------- offline: params */

int cmd_params(void)
{
	size_t count = 0;
	const theremini_param *params = theremini_params(&count);
	for (size_t i = 0; i < count; i++) {
		const theremini_param *p = &params[i];
		printf("%-4s cc=%-4d %s", p->id, p->cc, p->name);
		if (p->kind == THEREMINI_ENUM) {
			printf("  [enum: %d values]", p->value_count);
		} else if (p->kind == THEREMINI_TEXT) {
			printf("  [text]");
		} else {
			printf("  [%g .. %g]", p->min, p->max);
		}
		putchar('\n');
	}
	return 0;
}

/* -------------------------------------------------- library <-> device glue */

void library_preset_to_device(const theremaxi::Preset &src, theremini_preset *out)
{
	std::memset(out, 0, sizeof *out);
	size_t count = 0;
	const theremini_param *params = theremini_params(&count);
	for (size_t i = 0; i < count; i++) {
		const auto it = src.find(params[i].id);
		if (it == src.end()) {
			continue;
		}
		theremini_value *v = &out->values[i];
		v->present = true;
		if (params[i].kind == THEREMINI_TEXT) {
			std::strncpy(v->text, it->second.text.c_str(), THEREMINI_TEXT_MAX - 1);
		} else {
			v->number = it->second.as_number();
		}
	}
}

/* --------------------------------------------------------------- device I/O */

/* Open the sequencer, find a Theremini and connect. Prints and returns NULL on
 * failure. The caller owns the handle and must close it. */
theremini_alsa *connect(void)
{
	theremini_alsa *seq = theremini_alsa_open("theremini-ctl");
	if (!seq) {
		fprintf(stderr, "theremini-ctl: cannot open the ALSA sequencer\n");
		return NULL;
	}
	const char *name = theremini_alsa_discover(seq);
	if (!name) {
		fprintf(stderr, "theremini-ctl: no Theremini found\n");
		theremini_alsa_close(seq);
		return NULL;
	}
	printf("connected to: %s\n", name);
	return seq;
}

int cmd_identify(theremini_alsa *seq)
{
	uint8_t msg[THEREMINI_MESSAGE_MAX];
	uint8_t reply[64];
	size_t len = theremini_msg_identity_request(msg, sizeof msg);
	theremini_alsa_send(seq, msg, len);
	long n = theremini_alsa_read_sysex(seq, reply, sizeof reply, 1500);
	if (n >= 7 && reply[1] == 0x7e && reply[3] == 0x06 && reply[4] == 0x02) {
		printf("firmware %d.%d.%d\n", reply[n - 4], reply[n - 3], reply[n - 2]);
		return 0;
	}
	fprintf(stderr, "theremini-ctl: no identity reply (%ld bytes)\n", n);
	return 1;
}

int cmd_detect(theremini_alsa *seq)
{
	const int ch = theremini_alsa_detect_channel(seq, 2000);
	if (ch < 0) {
		fprintf(stderr, "theremini-ctl: could not detect the antenna channel - "
		                "move a hand near the antennas so the device streams\n");
		return 1;
	}
	printf("antennas stream on channel %d\n", ch);
	return 0;
}

/* Fetch a fresh dump into buf; returns its length, or -1 (message printed). */
long fetch_dump(theremini_alsa *seq, uint8_t *buf, size_t cap, theremini_dump *dump)
{
	uint8_t msg[THEREMINI_MESSAGE_MAX];
	size_t len = theremini_msg_request_all_presets(msg, sizeof msg);
	theremini_alsa_send(seq, msg, len);
	long n = theremini_alsa_read_sysex(seq, buf, cap, 3000);
	if (n <= 0) {
		fprintf(stderr, "theremini-ctl: no preset dump from the device (%ld)\n", n);
		return -1;
	}
	if (theremini_sysex_decode(buf, (size_t)n, dump) != THEREMINI_SYSEX_OK) {
		fprintf(stderr, "theremini-ctl: could not decode the preset dump\n");
		return -1;
	}
	return n;
}

int cmd_dump(theremini_alsa *seq)
{
	static uint8_t buf[16384];
	theremini_dump dump;
	long n = fetch_dump(seq, buf, sizeof buf, &dump);
	if (n < 0) {
		return 1;
	}
	printf("preset dump: %ld bytes, %zu presets\n", n, dump.count);
	return 0;
}

int cmd_backup(theremini_alsa *seq, const char *path)
{
	static uint8_t buf[16384];
	theremini_dump dump;
	long n = fetch_dump(seq, buf, sizeof buf, &dump);
	if (n < 0) {
		return 1;
	}
	FILE *f = fopen(path, "wb");
	if (!f || fwrite(buf, 1, (size_t)n, f) != (size_t)n) {
		fprintf(stderr, "theremini-ctl: cannot write %s\n", path);
		if (f) {
			fclose(f);
		}
		return 1;
	}
	fclose(f);
	printf("backed up %zu presets (%ld bytes) to %s\n", dump.count, n, path);
	return 0;
}

int cmd_restore(theremini_alsa *seq, int channel, int slot1, const char *path)
{
	const int slot = slot1 - 1; /* device slots are 0-based on the wire */
	FILE *f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "theremini-ctl: cannot read %s\n", path);
		return 1;
	}
	static uint8_t buf[16384];
	size_t blen = fread(buf, 1, sizeof buf, f);
	fclose(f);

	theremini_dump dump;
	if (theremini_sysex_decode(buf, blen, &dump) != THEREMINI_SYSEX_OK) {
		fprintf(stderr, "theremini-ctl: %s is not a valid preset dump\n", path);
		return 1;
	}
	if (slot < 0 || (size_t)slot >= dump.count) {
		fprintf(stderr, "theremini-ctl: the backup has no slot %d (it has %zu)\n",
		        slot1, dump.count);
		return 1;
	}
	if (!theremini_write_preset(seq, channel, slot, &dump.presets[slot])) {
		fprintf(stderr, "theremini-ctl: restore of slot %d failed\n", slot1);
		return 1;
	}
	printf("restored slot %d from %s\n", slot1, path);
	return 0;
}

int cmd_set_name(theremini_alsa *seq, int channel, int slot1, const char *name)
{
	const int slot = slot1 - 1;
	theremini_alsa_send_cc(seq, channel, 0, 0);
	theremini_alsa_send_program(seq, channel, slot);
	uint8_t msg[THEREMINI_MESSAGE_MAX];
	size_t len = theremini_msg_write_preset_name(name, msg, sizeof msg);
	theremini_alsa_send(seq, msg, len);
	theremini_alsa_send_cc(seq, channel, 119, 1); /* save */
	printf("set slot %d name to \"%s\" and saved\n", slot1, name);
	return 0;
}

int cmd_set(theremini_alsa *seq, int channel, int cc, double value)
{
	const theremini_param *p = theremini_param_by_cc(cc);
	if (p) {
		theremini_wire w;
		if (!theremini_value_export(p, value, &w)) {
			fprintf(stderr, "theremini-ctl: %s (cc %d) takes no numeric value\n",
			        p->name, cc);
			return 1;
		}
		theremini_alsa_send_cc(seq, channel, p->cc, w.bytes[0]);
		if (w.count == 2) {
			theremini_alsa_send_cc(seq, channel, p->lsb_cc, w.bytes[1]);
		}
		printf("sent %s = %g\n", p->name, value);
		return 0;
	}

	/* an unknown CC: send the value straight through, clamped to 7 bits */
	int raw = (int)value;
	if (raw < 0) {
		raw = 0;
	}
	if (raw > 127) {
		raw = 127;
	}
	theremini_alsa_send_cc(seq, channel, cc, raw);
	printf("sent cc %d = %d (raw; cc is not a known parameter)\n", cc, raw);
	return 0;
}

int cmd_send_preset(theremini_alsa *seq, int channel, const char *lib_path,
                    int index1, int slot1)
{
	theremaxi::Library lib;
	try {
		lib = theremaxi::load_theremaxi(lib_path);
	} catch (const std::exception &e) {
		fprintf(stderr, "theremini-ctl: cannot open %s: %s\n", lib_path, e.what());
		return 1;
	}
	if (index1 < 1 || (size_t)index1 > lib.presets.size()) {
		fprintf(stderr, "theremini-ctl: %s has no preset %d (it has %zu)\n",
		        lib_path, index1, lib.presets.size());
		return 1;
	}
	theremini_preset preset;
	library_preset_to_device(lib.presets[(size_t)index1 - 1], &preset);

	const int slot = slot1 - 1;
	if (!theremini_write_preset(seq, channel, slot, &preset)) {
		fprintf(stderr, "theremini-ctl: sending preset %d to slot %d failed\n",
		        index1, slot1);
		return 1;
	}
	printf("sent preset %d of %s to slot %d\n", index1, lib_path, slot1);
	return 0;
}

int cmd_sync(theremini_alsa *seq, const char *lib_path)
{
	static uint8_t buf[16384];
	theremini_dump dump;
	if (fetch_dump(seq, buf, sizeof buf, &dump) < 0) {
		return 1;
	}

	theremaxi::Library lib;
	size_t count = 0;
	const theremini_param *params = theremini_params(&count);
	for (size_t i = 0; i < dump.count; i++) {
		theremaxi::Preset preset;
		for (size_t j = 0; j < count; j++) {
			const theremini_value *v = &dump.presets[i].values[j];
			if (!v->present) {
				continue;
			}
			if (params[j].kind == THEREMINI_TEXT && v->text[0]) {
				preset[params[j].id] = theremaxi::Value::str(v->text);
			} else {
				preset[params[j].id] = theremaxi::Value::num(v->number);
			}
		}
		lib.presets.push_back(std::move(preset));
	}
	theremaxi::renumber(lib);

	try {
		theremaxi::save_theremaxi(lib_path, lib);
	} catch (const std::exception &e) {
		fprintf(stderr, "theremini-ctl: cannot write %s: %s\n", lib_path, e.what());
		return 1;
	}
	printf("synced %zu presets from the device into %s\n", lib.presets.size(), lib_path);
	return 0;
}

/* A device command needs a connection; run fn with one, then close it. */
template <typename Fn>
int with_device(Fn fn)
{
	theremini_alsa *seq = connect();
	if (!seq) {
		return 1;
	}
	const int rc = fn(seq);
	theremini_alsa_close(seq);
	return rc;
}

} // namespace

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0); /* so output survives a kill/timeout */

	/* a leading --channel N applies to the writing commands */
	int channel = 0;
	int i = 1;
	while (i < argc && strcmp(argv[i], "--channel") == 0) {
		if (i + 1 >= argc) {
			return usage_error("--channel needs a value");
		}
		channel = atoi(argv[i + 1]);
		if (channel < 0 || channel > 15) {
			return usage_error("--channel must be 0-15");
		}
		i += 2;
	}

	const char *cmd = i < argc ? argv[i] : "identify";
	i++;
	char **rest = argv + i;
	const int nrest = argc - i;

	if (strcmp(cmd, "help") == 0 || strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0) {
		fputs(kUsage, stdout);
		return 0;
	}
	if (strcmp(cmd, "params") == 0) {
		return cmd_params();
	}

	if (strcmp(cmd, "identify") == 0) {
		return with_device([](theremini_alsa *s) { return cmd_identify(s); });
	}
	if (strcmp(cmd, "detect") == 0) {
		return with_device([](theremini_alsa *s) { return cmd_detect(s); });
	}
	if (strcmp(cmd, "dump") == 0) {
		return with_device([](theremini_alsa *s) { return cmd_dump(s); });
	}
	if (strcmp(cmd, "backup") == 0) {
		if (nrest != 1) {
			return usage_error("backup needs a file name");
		}
		const char *path = rest[0];
		return with_device([path](theremini_alsa *s) { return cmd_backup(s, path); });
	}
	if (strcmp(cmd, "restore") == 0) {
		if (nrest != 2) {
			return usage_error("restore needs a slot and a backup file");
		}
		const int slot = atoi(rest[0]);
		const char *path = rest[1];
		if (slot < 1 || slot > 32) {
			return usage_error("slot must be 1-32");
		}
		return with_device([=](theremini_alsa *s) { return cmd_restore(s, channel, slot, path); });
	}
	if (strcmp(cmd, "set-name") == 0) {
		if (nrest != 2) {
			return usage_error("set-name needs a slot and a name");
		}
		const int slot = atoi(rest[0]);
		const char *name = rest[1];
		if (slot < 1 || slot > 32) {
			return usage_error("slot must be 1-32");
		}
		return with_device([=](theremini_alsa *s) { return cmd_set_name(s, channel, slot, name); });
	}
	if (strcmp(cmd, "set") == 0) {
		if (nrest != 2) {
			return usage_error("set needs a CC number and a value");
		}
		const int cc = atoi(rest[0]);
		const double value = atof(rest[1]);
		if (cc < 0 || cc > 127) {
			return usage_error("CC must be 0-127");
		}
		return with_device([=](theremini_alsa *s) { return cmd_set(s, channel, cc, value); });
	}
	if (strcmp(cmd, "send-preset") == 0) {
		/* send-preset <lib> <n> [--slot <slot>] */
		if (nrest < 2) {
			return usage_error("send-preset needs a library and a preset number");
		}
		const char *lib_path = rest[0];
		const int index = atoi(rest[1]);
		int slot = index; /* default: the same position on the device */
		if (nrest >= 4 && strcmp(rest[2], "--slot") == 0) {
			slot = atoi(rest[3]);
		} else if (nrest != 2) {
			return usage_error("send-preset options are just [--slot <slot>]");
		}
		if (index < 1) {
			return usage_error("the preset number must be 1 or more");
		}
		if (slot < 1 || slot > 32) {
			return usage_error("slot must be 1-32");
		}
		return with_device(
			[=](theremini_alsa *s) { return cmd_send_preset(s, channel, lib_path, index, slot); });
	}
	if (strcmp(cmd, "sync") == 0) {
		if (nrest != 1) {
			return usage_error("sync needs a library file name");
		}
		const char *lib_path = rest[0];
		return with_device([lib_path](theremini_alsa *s) { return cmd_sync(s, lib_path); });
	}

	return usage_error("unknown command");
}
