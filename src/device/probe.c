/* Guessing the antenna channel from the live stream. The device sends its
 * antennas as control-change on THEREMINI_VOLUME_CC and THEREMINI_PITCH_CC, so
 * the channel carrying the most of those is the one it is configured for.
 *
 * Copyright (C) 2017 Peter Niebling and contributors. GPL-3.0-or-later.
 */

#include "theremini/device.h"

#include <string.h>

/* enough antenna messages on one channel to trust it over noise */
#define THRESHOLD 3

void theremini_channel_probe_init(theremini_channel_probe *probe)
{
	memset(probe, 0, sizeof *probe);
}

void theremini_channel_probe_feed(theremini_channel_probe *probe, int channel, int cc)
{
	if (channel < 0 || channel > 15) {
		return;
	}
	if (cc == THEREMINI_VOLUME_CC || cc == THEREMINI_PITCH_CC) {
		probe->hits[channel]++;
	}
}

int theremini_channel_probe_result(const theremini_channel_probe *probe)
{
	int best = -1;
	int best_hits = 0;
	for (int ch = 0; ch < 16; ch++) {
		if (probe->hits[ch] > best_hits) {
			best_hits = probe->hits[ch];
			best = ch;
		}
	}
	return best_hits >= THRESHOLD ? best : -1;
}
