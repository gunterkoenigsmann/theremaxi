/* Antenna input reassembly - a transcription of lib/Input.pm. The golden
 * vectors in protocol/golden.json come from that perl, and tests/test_device.c
 * replays them here.
 *
 * Copyright (C) 2017 Peter Niebling and contributors. GPL-3.0-or-later.
 */

#include "theremini/device.h"

void theremini_input_init(theremini_input *in, theremini_input_config config)
{
	in->config = config;
	in->pending = -1; /* nothing held; note a held value of 0 is still >= 0 */
}

bool theremini_input_feed(theremini_input *in, int channel, int cc, int value, int *out)
{
	if (channel != in->config.channel) {
		return false; /* not for this antenna; pending untouched */
	}

	if (cc == in->config.cc) {
		if (in->config.wide) {
			in->pending = value; /* hold the high bits */
			return false;
		}
		in->pending = -1;
		*out = value; /* 7-bit: pass through */
		return true;
	}

	if (cc == in->config.cc + 32 && in->pending >= 0) {
		*out = (in->pending << 7) | value;
		in->pending = -1;
		return true;
	}

	return false;
}
