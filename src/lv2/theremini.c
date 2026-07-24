/* ThereMaxi LV2 plugin - no UI.
 *
 * Exposes the Theremini's parameters as control ports and emits the matching
 * MIDI control-change messages whenever a port changes, so a host such as
 * Ardour can automate the device from its timeline. The value-to-wire mapping
 * is libtheremini-protocol's; this file is only the LV2 glue and change
 * detection. run() does no allocation.
 *
 * Copyright (C) 2017 Peter Niebling and contributors. GPL-3.0-or-later.
 */

#include "theremini/protocol.h"
#include "lv2_ports.h"

#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/forge.h>
#include <lv2/midi/midi.h>
#include <lv2/urid/urid.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* control change on channel 0, matching the reference implementation */
#define CC_STATUS 0xb0

typedef struct {
	const float *control[THEREMINI_LV2_CONTROL_PORTS];
	float last[THEREMINI_LV2_CONTROL_PORTS];
	const theremini_param *param[THEREMINI_LV2_CONTROL_PORTS];

	LV2_Atom_Sequence *midi_out;

	LV2_URID_Map *map;
	LV2_URID midi_event;
	LV2_Atom_Forge forge;
} Plugin;

static LV2_Handle instantiate(const LV2_Descriptor *descriptor, double rate,
                              const char *bundle, const LV2_Feature *const *features)
{
	(void)descriptor;
	(void)rate;
	(void)bundle;

	Plugin *self = calloc(1, sizeof *self);
	if (!self) {
		return NULL;
	}

	for (const LV2_Feature *const *f = features; *f; f++) {
		if (strcmp((*f)->URI, LV2_URID__map) == 0) {
			self->map = (LV2_URID_Map *)(*f)->data;
		}
	}
	if (!self->map) { /* urid:map is required */
		free(self);
		return NULL;
	}

	self->midi_event = self->map->map(self->map->handle, LV2_MIDI__MidiEvent);
	lv2_atom_forge_init(&self->forge, self->map);

	for (unsigned i = 0; i < THEREMINI_LV2_CONTROL_PORTS; i++) {
		self->param[i] = theremini_param_by_id(theremini_lv2_port_param[i]);
		self->last[i] = NAN; /* so the first run emits the initial state */
	}

	return self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void *data)
{
	Plugin *self = instance;

	if (port < THEREMINI_LV2_CONTROL_PORTS) {
		self->control[port] = data;
	} else if (port == THEREMINI_LV2_MIDI_OUT_PORT) {
		self->midi_out = data;
	}
}

static void activate(LV2_Handle instance)
{
	Plugin *self = instance;
	for (unsigned i = 0; i < THEREMINI_LV2_CONTROL_PORTS; i++) {
		self->last[i] = NAN;
	}
}

/* one control-change message into the output sequence */
static void emit_cc(Plugin *self, uint8_t cc, uint8_t value)
{
	const uint8_t msg[3] = { CC_STATUS, cc, value };
	lv2_atom_forge_frame_time(&self->forge, 0);
	lv2_atom_forge_atom(&self->forge, sizeof msg, self->midi_event);
	lv2_atom_forge_write(&self->forge, msg, sizeof msg);
}

static void run(LV2_Handle instance, uint32_t sample_count)
{
	(void)sample_count;
	Plugin *self = instance;

	const uint32_t capacity = self->midi_out->atom.size;
	lv2_atom_forge_set_buffer(&self->forge, (uint8_t *)self->midi_out, capacity);

	LV2_Atom_Forge_Frame frame;
	lv2_atom_forge_sequence_head(&self->forge, &frame, 0);

	for (unsigned i = 0; i < THEREMINI_LV2_CONTROL_PORTS; i++) {
		if (!self->control[i] || !self->param[i]) {
			continue;
		}
		const float value = *self->control[i];
		if (value == self->last[i]) {
			continue; /* unchanged; NaN != NaN forces the first emit */
		}
		self->last[i] = value;

		theremini_wire wire;
		if (!theremini_value_export(self->param[i], value, &wire)) {
			continue;
		}

		const theremini_param *p = self->param[i];
		emit_cc(self, (uint8_t)p->cc, wire.bytes[0]);
		if (wire.count == 2) {
			emit_cc(self, (uint8_t)p->lsb_cc, wire.bytes[1]);
		}
	}

	lv2_atom_forge_pop(&self->forge, &frame);
}

static void cleanup(LV2_Handle instance)
{
	free(instance);
}

static const LV2_Descriptor descriptor = {
	.URI            = THEREMINI_LV2_URI,
	.instantiate    = instantiate,
	.connect_port   = connect_port,
	.activate       = activate,
	.run            = run,
	.deactivate     = NULL,
	.cleanup        = cleanup,
	.extension_data = NULL,
};

LV2_SYMBOL_EXPORT const LV2_Descriptor *lv2_descriptor(uint32_t index)
{
	return index == 0 ? &descriptor : NULL;
}
