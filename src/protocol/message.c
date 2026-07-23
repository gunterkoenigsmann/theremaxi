/* Building the device control messages.
 *
 * The fixed bytes come from the generated templates; the framing and the name
 * encoding are here. theremini_name_encode mirrors name::value_export, and
 * tests/test_message.c checks every message against bytes the perl produced.
 *
 * Copyright (C) 2017 Peter Niebling and contributors. GPL-3.0-or-later.
 */

#include "theremini/protocol.h"
#include "internal.h"

#include <string.h>

void theremini_name_encode(const char *name, uint8_t out[THEREMINI_NAME_BYTES])
{
	memset(out, ' ', THEREMINI_NAME_BYTES);

	if (!name) {
		return;
	}

	/* trim leading and trailing spaces, the way name::value_import/export do */
	const char *start = name;
	while (*start == ' ') {
		start++;
	}
	size_t len = strlen(start);
	while (len > 0 && start[len - 1] == ' ') {
		len--;
	}
	if (len > THEREMINI_NAME_BYTES) {
		len = THEREMINI_NAME_BYTES;
	}

	memcpy(out, start, len);
}

/* F0 + payload + F7 into out, or 0 if it will not fit. */
static size_t frame(const uint8_t *payload, size_t payload_len,
                    uint8_t *out, size_t cap)
{
	const size_t total = payload_len + 2;
	if (cap < total) {
		return 0;
	}
	out[0] = 0xf0;
	memcpy(out + 1, payload, payload_len);
	out[total - 1] = 0xf7;
	return total;
}

static size_t build_constant(const uint8_t *(*tmpl)(size_t *),
                             uint8_t *out, size_t cap)
{
	size_t len;
	const uint8_t *payload = tmpl(&len);
	return frame(payload, len, out, cap);
}

static size_t build_name(const uint8_t *(*prefix_tmpl)(size_t *),
                         const uint8_t *(*suffix_tmpl)(size_t *),
                         const char *name, uint8_t *out, size_t cap)
{
	size_t prefix_len, suffix_len;
	const uint8_t *prefix = prefix_tmpl(&prefix_len);
	const uint8_t *suffix = suffix_tmpl(&suffix_len);

	uint8_t payload[THEREMINI_MESSAGE_MAX];
	const size_t payload_len = prefix_len + THEREMINI_NAME_BYTES + suffix_len;
	if (payload_len > sizeof payload) {
		return 0;
	}

	uint8_t *p = payload;
	memcpy(p, prefix, prefix_len);
	p += prefix_len;
	theremini_name_encode(name, p);
	p += THEREMINI_NAME_BYTES;
	memcpy(p, suffix, suffix_len);

	return frame(payload, payload_len, out, cap);
}

size_t theremini_msg_identity_request(uint8_t *out, size_t cap)
{
	return build_constant(theremini_tmpl_identity_request, out, cap);
}

size_t theremini_msg_request_all_presets(uint8_t *out, size_t cap)
{
	return build_constant(theremini_tmpl_request_all_presets, out, cap);
}

size_t theremini_msg_write_preset_name(const char *name, uint8_t *out, size_t cap)
{
	return build_name(theremini_tmpl_preset_name_prefix,
	                  theremini_tmpl_preset_name_suffix, name, out, cap);
}

size_t theremini_msg_write_effect_name(const char *name, uint8_t *out, size_t cap)
{
	return build_name(theremini_tmpl_effect_name_prefix,
	                  theremini_tmpl_effect_name_suffix, name, out, cap);
}
