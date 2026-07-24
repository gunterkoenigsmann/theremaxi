/* Recognising the device by its ALSA client name - the /theremini/i match the
 * reference implementation uses.
 *
 * Copyright (C) 2017 Peter Niebling and contributors. GPL-3.0-or-later.
 */

#include "theremini/device.h"

#include <ctype.h>

bool theremini_client_matches(const char *name)
{
	static const char needle[] = "theremini";

	if (!name) {
		return false;
	}

	for (const char *start = name; *start; start++) {
		const char *n = needle;
		const char *s = start;
		while (*n && tolower((unsigned char)*s) == *n) {
			s++;
			n++;
		}
		if (!*n) {
			return true;
		}
	}
	return false;
}
