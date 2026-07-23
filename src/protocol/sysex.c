/* Undoing the device's seven-bit packing, and framing a whole dump.
 *
 * A transcription of ThereMaxi::Preset::sysex and _sx_. protocol/golden.json
 * carries both the per-group unpacking and whole framed messages run through
 * that perl, and tests/test_sysex.c replays them here.
 *
 * Copyright (C) 2017 Peter Niebling and contributors. GPL-3.0-or-later.
 */

#include "theremini/protocol.h"

#include <string.h>

/* A preset's 174 packed bytes unpack, three-into-two, to 116 = 0x74 bytes,
 * which is exactly what theremini_preset_decode reads. */
#define PRESET_BYTES 0x74

/* One F0, then a 22-byte header, then the body. */
#define HEADER_BYTES 22

uint16_t theremini_sysex_unpack3(uint8_t b0, uint8_t b1, uint8_t b2)
{
	/* A faithful copy of _sx_, mutating v in the same order it does, so the
	 * bit arithmetic matches step for step rather than by a cleaner formula
	 * that might not. */
	uint32_t v = (uint32_t)b0 << 16 | (uint32_t)b1 << 8 | (uint32_t)b2;

	uint32_t r = v & 0x3f;
	v &= 0xfffc0;
	v >>= 2;
	r |= v & 0x3fff;
	v &= 0x3c000;
	v >>= 2;

	return (uint16_t)(v | r);
}

/* Unpack one preset's packed bytes into decoder-ready bytes. Reads three at a
 * time, writes two, little end first, and stops once it has the 0x74 the
 * decoder needs - trailing packed bytes beyond that are not used, matching the
 * perl, whose regexp also drops an incomplete trailing group. */
static bool unpack_preset(const uint8_t *packed, size_t packed_len,
                          uint8_t out[PRESET_BYTES])
{
	size_t o = 0;

	for (size_t i = 0; i + 3 <= packed_len && o + 2 <= PRESET_BYTES; i += 3) {
		const uint16_t v = theremini_sysex_unpack3(packed[i], packed[i + 1],
		                                           packed[i + 2]);
		out[o++] = (uint8_t)(v & 0xff);
		out[o++] = (uint8_t)(v >> 8);
	}

	return o >= PRESET_BYTES;
}

theremini_sysex_status theremini_sysex_decode(const uint8_t *data, size_t size,
                                              theremini_dump *out)
{
	if (!data || !out) {
		return THEREMINI_SYSEX_BAD_HEADER;
	}

	memset(out, 0, sizeof *out);

	/* strip the enclosing f0 .. f7 if it is there */
	if (size >= 2 && data[0] == 0xf0 && data[size - 1] == 0xf7) {
		data += 1;
		size -= 2;
	}

	if (size < HEADER_BYTES) {
		return THEREMINI_SYSEX_TRUNCATED;
	}

	/* the third and fourth header bytes choose the layout */
	const uint8_t kind = data[2];
	const uint8_t one = data[3];

	const uint8_t *body = data + HEADER_BYTES;
	const size_t body_len = size - HEADER_BYTES;

	size_t preset_packed; /* packed bytes per preset */
	if (one != 0x01) {
		return THEREMINI_SYSEX_BAD_HEADER;
	}
	if (kind == 0x01) {
		preset_packed = body_len / 32; /* all thirty-two slots */
	} else if (kind == 0x04 || kind == 0x05) {
		preset_packed = body_len;      /* a single preset */
	} else {
		return THEREMINI_SYSEX_BAD_HEADER;
	}

	if (preset_packed == 0) {
		return THEREMINI_SYSEX_TRUNCATED;
	}

	for (size_t at = 0; at < body_len; at += preset_packed) {
		if (at + preset_packed > body_len) {
			return THEREMINI_SYSEX_TRUNCATED;
		}
		if (out->count >= THEREMINI_MAX_PRESETS) {
			return THEREMINI_SYSEX_TOO_MANY;
		}

		uint8_t blob[PRESET_BYTES];
		if (!unpack_preset(body + at, preset_packed, blob)) {
			return THEREMINI_SYSEX_TRUNCATED;
		}

		/* the preset's own number comes out of its bytes, so pass -1 here */
		if (!theremini_preset_decode(blob, sizeof blob, -1,
		                             &out->presets[out->count])) {
			return THEREMINI_SYSEX_TRUNCATED;
		}
		out->count++;
	}

	return THEREMINI_SYSEX_OK;
}
