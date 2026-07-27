/* A small command-line tool that exercises the ALSA transport against a real
 * Theremini: discover it, ask its identity, and optionally count the presets in
 * a dump. It deliberately prints only counts and the firmware version, never the
 * preset contents, which are the device maker's.
 *
 *   theremini-probe            discover and identify
 *   theremini-probe --dump     also fetch the preset dump and report the count
 *   theremini-probe --antenna  print a few live antenna readings, then stop
 *
 * Copyright (C) 2017 Peter Niebling and contributors. GPL-3.0-or-later.
 */

#include "theremini/alsa.h"
#include "theremini/device.h"
#include "theremini/protocol.h"
#include "theremini/write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int antenna_left = 0;

static void on_cc(int channel, int cc, int value, void *user)
{
	(void)user;
	if (antenna_left > 0) {
		printf("  antenna: channel %d controller %d value %d\n", channel, cc, value);
		antenna_left--;
	}
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0); /* so output survives a kill/timeout */

	const bool want_dump = argc > 1 && strcmp(argv[1], "--dump") == 0;
	const bool want_antenna = argc > 1 && strcmp(argv[1], "--antenna") == 0;
	const bool want_channel = argc > 1 && strcmp(argv[1], "--channel") == 0;
	const char *backup_path = (argc > 2 && strcmp(argv[1], "--backup") == 0) ? argv[2] : NULL;
	/* --set-name SLOT NAME : select the 0-based slot, set its name, save */
	const bool want_set_name = argc > 3 && strcmp(argv[1], "--set-name") == 0;
	/* --restore-slot SLOT BACKUP.syx : write the backup's preset for that slot back */
	const bool want_restore = argc > 3 && strcmp(argv[1], "--restore-slot") == 0;

	theremini_alsa *seq = theremini_alsa_open("ThereMaxi-probe");
	if (!seq) {
		fprintf(stderr, "cannot open the ALSA sequencer\n");
		return 1;
	}

	const char *name = theremini_alsa_discover(seq);
	if (!name) {
		fprintf(stderr, "no Theremini found\n");
		theremini_alsa_close(seq);
		return 1;
	}
	printf("connected to: %s\n", name);

	uint8_t msg[THEREMINI_MESSAGE_MAX];
	uint8_t reply[8192];

	/* identity */
	size_t len = theremini_msg_identity_request(msg, sizeof msg);
	theremini_alsa_send(seq, msg, len);
	long n = theremini_alsa_read_sysex(seq, reply, sizeof reply, 1500);
	/* reply is F0 7E 7F 06 02 ... <v v v> F7 */
	if (n >= 7 && reply[1] == 0x7e && reply[3] == 0x06 && reply[4] == 0x02) {
		printf("identity ok, firmware %d.%d.%d\n", reply[n - 4], reply[n - 3], reply[n - 2]);
	} else {
		printf("no identity reply (%ld bytes)\n", n);
	}

	if (want_dump) {
		len = theremini_msg_request_all_presets(msg, sizeof msg);
		theremini_alsa_send(seq, msg, len);
		n = theremini_alsa_read_sysex(seq, reply, sizeof reply, 3000);
		if (n <= 0) {
			printf("no preset dump (%ld)\n", n);
		} else {
			theremini_dump dump;
			const theremini_sysex_status st = theremini_sysex_decode(reply, (size_t)n, &dump);
			if (st == THEREMINI_SYSEX_OK) {
				printf("preset dump: %ld bytes, decoded %zu presets\n", n, dump.count);
			} else {
				printf("preset dump: %ld bytes, decode failed (%d)\n", n, st);
			}
		}
	}

	if (backup_path) {
		/* save the exact bytes the device sends, as a restore point taken before
		 * anything writes to the device. Decode it too, only to confirm it is a
		 * whole, valid dump. */
		len = theremini_msg_request_all_presets(msg, sizeof msg);
		theremini_alsa_send(seq, msg, len);
		n = theremini_alsa_read_sysex(seq, reply, sizeof reply, 3000);

		theremini_dump dump;
		if (n <= 0 || theremini_sysex_decode(reply, (size_t)n, &dump) != THEREMINI_SYSEX_OK) {
			printf("backup failed: did not get a valid dump (%ld)\n", n);
			theremini_alsa_close(seq);
			return 1;
		}
		FILE *f = fopen(backup_path, "wb");
		if (!f || fwrite(reply, 1, (size_t)n, f) != (size_t)n) {
			printf("backup failed: cannot write %s\n", backup_path);
			if (f) {
				fclose(f);
			}
			theremini_alsa_close(seq);
			return 1;
		}
		fclose(f);
		printf("backed up %zu presets (%ld bytes) to %s\n", dump.count, n, backup_path);
	}

	if (want_set_name) {
		const int slot = atoi(argv[2]);
		theremini_alsa_send_cc(seq, 0, 0, 0);
		theremini_alsa_send_program(seq, 0, slot);
		uint8_t name_msg[THEREMINI_MESSAGE_MAX];
		size_t mlen = theremini_msg_write_preset_name(argv[3], name_msg, sizeof name_msg);
		theremini_alsa_send(seq, name_msg, mlen);
		theremini_alsa_send_cc(seq, 0, 119, 1); /* save */
		printf("set slot %d name to \"%s\" and saved\n", slot, argv[3]);
	}

	if (want_restore) {
		const int slot = atoi(argv[2]);
		FILE *f = fopen(argv[3], "rb");
		if (!f) {
			printf("cannot read %s\n", argv[3]);
			theremini_alsa_close(seq);
			return 1;
		}
		static uint8_t buf[16384];
		size_t blen = fread(buf, 1, sizeof buf, f);
		fclose(f);
		theremini_dump dump;
		if (theremini_sysex_decode(buf, blen, &dump) != THEREMINI_SYSEX_OK ||
		    slot < 0 || (size_t)slot >= dump.count) {
			printf("backup does not have slot %d\n", slot);
			theremini_alsa_close(seq);
			return 1;
		}
		if (theremini_write_preset(seq, 0, slot, &dump.presets[slot])) {
			printf("restored slot %d from %s\n", slot, argv[3]);
		} else {
			printf("restore of slot %d failed\n", slot);
		}
	}

	if (want_channel) {
		const int ch = theremini_alsa_detect_channel(seq, 2000);
		if (ch >= 0) {
			printf("antennas stream on channel %d\n", ch);
		} else {
			printf("could not detect the antenna channel (move a hand near the "
			       "antennas so the device streams)\n");
		}
	}

	if (want_antenna) {
		antenna_left = 6;
		theremini_alsa_on_cc(seq, on_cc, NULL);
		while (antenna_left > 0) {
			if (theremini_alsa_pump(seq, 2000) <= 0) {
				break; /* nothing arriving */
			}
		}
	}

	theremini_alsa_close(seq);
	return 0;
}
