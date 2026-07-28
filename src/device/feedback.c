/* The MidiFeedbackLoop mapping - an antenna's live value driving a preset
 * parameter. A transcription of the _loop_ and the range map in the perl
 * Feature/MidiFeedbackLoop.pm: gate the value, ignore small changes, optionally
 * reverse it, and scale it linearly onto the target's range.
 *
 * Copyright (C) 2017 Peter Niebling and contributors. GPL-3.0-or-later.
 */

#include "theremini/device.h"

#include <stdlib.h>

bool theremini_feedback_feed(theremini_feedback *fb, int value, double *out)
{
	if (!fb->enabled) {
		return false;
	}
	if (value < fb->low || value > fb->high) {
		return false; /* outside the gate; last is left as it was */
	}

	/* the perl uses ($lastval || $value): a stored 0 counts as "no last" */
	const int last = fb->last > 0 ? fb->last : value;
	fb->last = value;

	if (abs(last - value) < fb->sens) {
		return false;
	}

	int idx = fb->revert ? fb->input_max - value : value;
	if (idx < 0) {
		idx = 0;
	}
	if (idx > fb->input_max) {
		idx = fb->input_max;
	}

	*out = (double)idx * (fb->out_max - fb->out_min) / fb->input_max + fb->out_min;
	return true;
}
