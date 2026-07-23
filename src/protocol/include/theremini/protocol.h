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

/* An upper bound, not the actual count: ask theremini_params() for that. */
#define THEREMINI_PARAM_MAX 64
#define THEREMINI_TEXT_MAX 16 /* 13 characters and a terminator */

typedef enum {
	THEREMINI_NUMERIC, /* a range with a unit: Hz, %, ms, semitones */
	THEREMINI_ENUM,    /* one of a fixed list of names */
	THEREMINI_TEXT     /* a name typed by the user */
} theremini_kind;

/* How a value read out of a preset dump becomes the value shown.
 * Resolved from the perl class hierarchy by tools/dump-protocol.pl. */
typedef enum {
	THEREMINI_IMPORT_IDENTITY, /* taken as it is - and not range checked */
	THEREMINI_IMPORT_NUMERIC,  /* rounded to the parameter's decimals, clamped */
	THEREMINI_IMPORT_TEXT,     /* trimmed, cut to 13 characters */
	THEREMINI_IMPORT_CUTOFF,   /* the filter cutoff's cube root curve */
	THEREMINI_IMPORT_SUM       /* two words added: the waveform */
} theremini_import;

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
	theremini_import import;
} theremini_param;

/* The parameter table. Stable order, sorted by id. */
const theremini_param *theremini_params(size_t *count);
const theremini_param *theremini_param_by_id(const char *id);
const theremini_param *theremini_param_by_cc(int cc);
size_t theremini_param_index(const theremini_param *param);

/* Where a parameter sits in a preset dump. */
typedef enum {
	THEREMINI_PACK_U16,        /* two bytes, low first */
	THEREMINI_PACK_S16,        /* the same, signed */
	THEREMINI_PACK_TEXT_PADDED,/* 13 characters, each followed by a filler byte */
	THEREMINI_PACK_TEXT        /* 13 bytes, terminated by a zero */
} theremini_pack;

typedef struct {
	size_t offset;
	const char *param_id;
	theremini_pack pack;
	double divisor; /* 0 when the raw number is used as it is */
} theremini_offset;

const theremini_offset *theremini_offsets(size_t *count);

/* A decoded preset. Values are indexed like theremini_params(). */
typedef struct {
	double number;                   /* NUMERIC and ENUM */
	char text[THEREMINI_TEXT_MAX];   /* TEXT */
	bool present;
} theremini_value;

typedef struct {
	theremini_value values[THEREMINI_PARAM_MAX];
} theremini_preset;

/* Decode one preset out of a sysex dump.
 *
 * size must cover the whole preset; number is the slot it came from, or -1.
 * Returns false if the data is too short - the one place a malformed dump can
 * be rejected rather than trusted. */
bool theremini_preset_decode(const uint8_t *data, size_t size, int number,
                             theremini_preset *out);

const theremini_value *theremini_preset_value(const theremini_preset *preset,
                                              const char *id);

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
