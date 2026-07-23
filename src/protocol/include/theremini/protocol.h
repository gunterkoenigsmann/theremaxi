/* libtheremini-protocol - what the Theremini's parameters are and how their
 * values turn into bytes.
 *
 * No I/O, no threads, no allocation: everything here is safe to call from an
 * LV2 run() callback. The parameter table is generated from protocol/tables.json,
 * which is itself generated from the perl implementation - see DESIGN.md.
 *
 * Copyright (C) 2017 Peter Niebling and contributors. GPL-3.0-or-later.
 */

#ifndef THEREMINI_PROTOCOL_H
#define THEREMINI_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	THEREMINI_NUMERIC, /* a range with a unit: Hz, %, ms, semitones */
	THEREMINI_ENUM,    /* one of a fixed list of names */
	THEREMINI_TEXT     /* a name typed by the user */
} theremini_kind;

typedef struct {
	const char *id;    /* "74", or "_ps" for the parameters that are not a CC */
	const char *name;
	theremini_kind kind;

	int cc;            /* -1 when id is not a controller number */
	int lsb_cc;        /* the CC carrying the low bits, or -1 */
	int bits;          /* 7 or 14 */

	double min;
	double max;
	int digits;        /* decimals the value is displayed with */
	const char *format;

	const char *const *values; /* names for THEREMINI_ENUM, else NULL */
	int value_count;

	bool in_preset;    /* stored in a preset, as opposed to global */
} theremini_param;

/* The parameter table. Stable order, sorted by id. */
const theremini_param *theremini_params(size_t *count);
const theremini_param *theremini_param_by_id(const char *id);
const theremini_param *theremini_param_by_cc(int cc);

/* A value on the wire: one byte for a 7-bit parameter, two for a 14-bit one,
 * high bits first. Send bytes[0] as cc and bytes[1] as lsb_cc. */
typedef struct {
	uint8_t bytes[2];
	uint8_t count;
} theremini_wire;

/* Convert a displayed value to the bytes that go out.
 * Returns false for parameters that have no numeric wire form (THEREMINI_TEXT).
 *
 * Note that count can be 1 even for a 14-bit parameter: at the very ends of the
 * range the reference implementation sends the high controller alone. See the
 * "Decisions already forced by the data" section of DESIGN.md. */
bool theremini_value_export(const theremini_param *param, double value,
                            theremini_wire *out);

#ifdef __cplusplus
}
#endif

#endif /* THEREMINI_PROTOCOL_H */
