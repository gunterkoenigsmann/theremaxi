/* Drives the real LV2 plugin: instantiates it, runs it, and checks that the
 * MIDI it emits matches what the protocol library says each control value
 * should send. No host and no hardware - just the plugin's own descriptor. */

#define _POSIX_C_SOURCE 200809L /* strdup */

#include "theremini/protocol.h"
#include "lv2_ports.h"

#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/midi/midi.h>
#include <lv2/urid/urid.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

const LV2_Descriptor *lv2_descriptor(uint32_t index);

/* A minimal URID map: stable ids, first-come. */
#define MAX_URIS 64
static char *uris[MAX_URIS];
static uint32_t uri_count;

static LV2_URID map_uri(LV2_URID_Map_Handle handle, const char *uri)
{
	(void)handle;
	for (uint32_t i = 0; i < uri_count; i++) {
		if (strcmp(uris[i], uri) == 0) {
			return i + 1;
		}
	}
	uris[uri_count] = strdup(uri);
	return ++uri_count;
}

/* Collect the control-change messages from a run. */
typedef struct { uint8_t cc, value; } cc_event;

static size_t collect(const LV2_Atom_Sequence *seq, LV2_URID midi_event,
                      cc_event *out, size_t max)
{
	size_t n = 0;
	LV2_ATOM_SEQUENCE_FOREACH (seq, ev) {
		if (ev->body.type != midi_event) {
			continue;
		}
		const uint8_t *m = (const uint8_t *)(&ev->body + 1);
		if ((m[0] & 0xf0) == 0xb0 && n < max) {
			out[n].cc = m[1];
			out[n].value = m[2];
			n++;
		}
	}
	return n;
}

int main(void)
{
	const LV2_Descriptor *desc = lv2_descriptor(0);
	if (!desc || strcmp(desc->URI, THEREMINI_LV2_URI) != 0) {
		printf("FAIL: no descriptor\n");
		return 1;
	}

	LV2_URID_Map map = { NULL, map_uri };
	LV2_Feature map_feature = { LV2_URID__map, &map };
	const LV2_Feature *features[] = { &map_feature, NULL };
	const LV2_URID midi_event = map_uri(NULL, LV2_MIDI__MidiEvent);

	LV2_Handle h = desc->instantiate(desc, 48000.0, "", features);
	if (!h) {
		printf("FAIL: instantiate returned NULL\n");
		return 1;
	}

	float control[THEREMINI_LV2_CONTROL_PORTS];
	for (unsigned i = 0; i < THEREMINI_LV2_CONTROL_PORTS; i++) {
		const theremini_param *p = theremini_param_by_id(theremini_lv2_port_param[i]);
		control[i] = (float)p->min;
		desc->connect_port(h, i, &control[i]);
	}

	uint8_t buf[4096];
	LV2_Atom_Sequence *seq = (LV2_Atom_Sequence *)buf;
	desc->connect_port(h, THEREMINI_LV2_MIDI_OUT_PORT, seq);
	desc->activate(h);

	cc_event events[128];

	/* First run: every port starts changed, so each emits its value. */
	seq->atom.size = sizeof buf;
	desc->run(h, 32);
	size_t n = collect(seq, midi_event, events, 128);

	size_t expected = 0;
	for (unsigned i = 0; i < THEREMINI_LV2_CONTROL_PORTS; i++) {
		const theremini_param *p = theremini_param_by_id(theremini_lv2_port_param[i]);
		theremini_wire w;
		theremini_value_export(p, control[i], &w);
		expected += w.count;
	}
	if (n != expected) {
		printf("FAIL: first run emitted %zu messages, expected %zu\n", n, expected);
		failures++;
	} else {
		printf("first run: %zu control changes for %d ports\n", n,
		       THEREMINI_LV2_CONTROL_PORTS);
	}

	/* Second run, nothing changed: silence. */
	seq->atom.size = sizeof buf;
	desc->run(h, 32);
	n = collect(seq, midi_event, events, 128);
	if (n != 0) {
		printf("FAIL: an unchanged run emitted %zu messages\n", n);
		failures++;
	} else {
		printf("unchanged run is silent\n");
	}

	/* Change one 14-bit port (delay time, CC 12): expect its two messages. */
	for (unsigned i = 0; i < THEREMINI_LV2_CONTROL_PORTS; i++) {
		if (strcmp(theremini_lv2_port_param[i], "12") == 0) {
			const theremini_param *p = theremini_param_by_id("12");
			control[i] = (float)(p->max / 2.0);
			seq->atom.size = sizeof buf;
			desc->run(h, 32);
			n = collect(seq, midi_event, events, 128);

			theremini_wire w;
			theremini_value_export(p, control[i], &w);
			if (n != w.count || events[0].cc != p->cc ||
			    (w.count == 2 && events[1].cc != p->lsb_cc)) {
				printf("FAIL: changing CC 12 emitted %zu messages (cc %d..)\n",
				       n, n ? events[0].cc : -1);
				failures++;
			} else {
				printf("changing delay time emits %zu message(s) on CC %d/%d\n",
				       n, p->cc, p->lsb_cc);
			}
			break;
		}
	}

	desc->cleanup(h);
	for (uint32_t i = 0; i < uri_count; i++) {
		free(uris[i]);
	}

	if (failures) {
		printf("\n%d check(s) failed\n", failures);
		return 1;
	}
	printf("\nall checks passed\n");
	return 0;
}
