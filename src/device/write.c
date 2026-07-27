/* Writing a preset to the device - see write.h. A transcription of the perl
 * app's save sequence (Device.pm: _send_, midi_ps, midi_send): select the slot,
 * send each value as CC and each name as sysex, then CC 119 to save.
 *
 * Copyright (C) 2017 Peter Niebling and contributors. GPL-3.0-or-later.
 */

#include "theremini/write.h"

#include <string.h>

/* CC 119 = "save to the current preset" */
#define CC_SAVE 119

bool theremini_write_preset(theremini_alsa *seq, int channel, int slot,
                            const theremini_preset *preset)
{
	/* select the slot: bank select 0, then program change (perl's midi_ps) */
	if (!theremini_alsa_send_cc(seq, channel, 0, 0) ||
	    !theremini_alsa_send_program(seq, channel, slot)) {
		return false;
	}

	size_t count = 0;
	const theremini_param *params = theremini_params(&count);

	for (size_t i = 0; i < count; i++) {
		const theremini_param *p = &params[i];
		const theremini_value *v = &preset->values[i];
		if (!v->present) {
			continue;
		}

		if (p->kind == THEREMINI_TEXT) {
			uint8_t msg[THEREMINI_MESSAGE_MAX];
			size_t len = 0;
			if (strcmp(p->id, "_ps") == 0) {
				len = theremini_msg_write_preset_name(v->text, msg, sizeof msg);
			} else if (strcmp(p->id, "_fx") == 0) {
				len = theremini_msg_write_effect_name(v->text, msg, sizeof msg);
			}
			if (len && !theremini_alsa_send(seq, msg, len)) {
				return false;
			}
		} else if (p->cc >= 0) {
			theremini_wire w;
			if (!theremini_value_export(p, v->number, &w)) {
				continue;
			}
			if (!theremini_alsa_send_cc(seq, channel, p->cc, w.bytes[0])) {
				return false;
			}
			if (w.count == 2 &&
			    !theremini_alsa_send_cc(seq, channel, p->lsb_cc, w.bytes[1])) {
				return false;
			}
		}
	}

	return theremini_alsa_send_cc(seq, channel, CC_SAVE, 1);
}
