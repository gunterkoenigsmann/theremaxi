/**
 * @file protocol.h
 * @brief What the Theremini's parameters are and how their values turn into bytes.
 *
 * This library knows the parameter set, how a sysex preset dump is laid out, and
 * how a displayed value (Hz, %, ms, a scale name) maps to the MIDI bytes that go
 * to the device. It does no I/O, spawns no threads, and does not allocate, so
 * every function here is safe to call from an LV2 `run()` callback.
 *
 * The tables are generated from `protocol/tables.json`, which is itself generated
 * from the perl reference implementation - see DESIGN.md. Nothing here is written
 * by hand.
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

/** Upper bound on the parameter count, for fixed-size arrays. The real count
 *  comes from theremini_params(). */
#define THEREMINI_PARAM_MAX 64

/** Buffer size for a parameter name: 13 characters and a terminator. */
#define THEREMINI_TEXT_MAX 16

/** What kind of value a parameter holds. */
typedef enum {
	THEREMINI_NUMERIC, /**< a range with a unit: Hz, %, ms, semitones */
	THEREMINI_ENUM,    /**< one of a fixed list of names */
	THEREMINI_TEXT     /**< a name typed by the user */
} theremini_kind;

/** How a value read out of a preset dump becomes the value shown. Resolved from
 *  the perl class hierarchy by tools/dump-protocol.pl. */
typedef enum {
	THEREMINI_IMPORT_IDENTITY, /**< taken as it is, and not range checked */
	THEREMINI_IMPORT_NUMERIC,  /**< rounded to the parameter's decimals, then clamped */
	THEREMINI_IMPORT_TEXT,     /**< trimmed, cut to 13 characters */
	THEREMINI_IMPORT_CUTOFF,   /**< the filter cutoff's cube-root curve */
	THEREMINI_IMPORT_SUM       /**< two words added together: the waveform */
} theremini_import;

/** One editable parameter. */
typedef struct {
	const char *id;    /**< "74", or "_ps" for the parameters that are not a CC */
	const char *name;
	theremini_kind kind;

	int cc;            /**< controller number, or -1 when id is not one */
	int lsb_cc;        /**< the CC carrying the low bits of a 14-bit value, or -1 */
	int bits;          /**< 7 or 14 */

	double min;
	double max;
	int digits;        /**< decimals the value is displayed with */
	const char *format; /**< printf format for the displayed value, may be NULL */

	const char *const *values; /**< names, for THEREMINI_ENUM; NULL otherwise */
	int value_count;

	bool in_preset;    /**< stored in a preset, as opposed to a global setting */
	theremini_import import;
} theremini_param;

/**
 * @brief The parameter table, in a stable order sorted by id.
 * @param count filled in with the number of parameters, unless NULL.
 * @return a pointer to the first parameter; the table is static and never freed.
 */
const theremini_param *theremini_params(size_t *count);

/** @brief Find a parameter by its id ("74", "_ps"), or NULL. */
const theremini_param *theremini_param_by_id(const char *id);

/** @brief Find a parameter by its controller number, or NULL. */
const theremini_param *theremini_param_by_cc(int cc);

/** @brief The index of a parameter within theremini_params(), used to line the
 *         table up with a decoded preset's values. */
size_t theremini_param_index(const theremini_param *param);

/** Where a parameter sits in a preset dump, and how its bytes are read. */
typedef enum {
	THEREMINI_PACK_U16,         /**< two bytes, low first */
	THEREMINI_PACK_S16,         /**< the same, signed */
	THEREMINI_PACK_TEXT_PADDED, /**< 13 characters, each followed by a filler byte */
	THEREMINI_PACK_TEXT         /**< up to 13 bytes, terminated by a zero */
} theremini_pack;

/** One field of a preset dump: which parameter it feeds and how to read it. */
typedef struct {
	size_t offset;
	const char *param_id;
	theremini_pack pack;
	double divisor; /**< raw value is divided by this; 0 means use it as it is */
} theremini_offset;

/**
 * @brief The sysex offset table.
 * @param count filled in with the number of offsets, unless NULL.
 */
const theremini_offset *theremini_offsets(size_t *count);

/** One decoded parameter value. Which member is meaningful follows the
 *  parameter's #theremini_kind. */
typedef struct {
	double number;                 /**< for THEREMINI_NUMERIC and THEREMINI_ENUM */
	char text[THEREMINI_TEXT_MAX]; /**< for THEREMINI_TEXT */
	bool present;                  /**< false if the dump did not carry this one */
} theremini_value;

/** A decoded preset. Its values are indexed like theremini_params(); reach one
 *  by name with theremini_preset_value(). */
typedef struct {
	theremini_value values[THEREMINI_PARAM_MAX];
} theremini_preset;

/**
 * @brief Decode one preset out of a sysex dump.
 * @param data   the preset's bytes.
 * @param size   how many bytes @p data holds; must cover the whole preset.
 * @param number the slot the preset came from, or -1 if unknown.
 * @param out    filled in on success.
 * @return false if the data is too short. This is the one place a malformed
 *         dump is rejected rather than trusted.
 */
bool theremini_preset_decode(const uint8_t *data, size_t size, int number,
                             theremini_preset *out);

/** @brief The value a decoded preset holds for a parameter id, or NULL if there
 *         is no such parameter. Check @c present to tell "decoded as zero" from
 *         "not in the dump". */
const theremini_value *theremini_preset_value(const theremini_preset *preset,
                                              const char *id);

/** A value ready for the wire: one byte for a 7-bit parameter, two for a 14-bit
 *  one with the high bits first. Send @c bytes[0] as the parameter's cc and, when
 *  @c count is 2, @c bytes[1] as its lsb_cc. */
typedef struct {
	uint8_t bytes[2];
	uint8_t count;
} theremini_wire;

/**
 * @brief Convert a displayed value to the bytes that go to the device.
 * @param param the parameter the value belongs to.
 * @param value the value as shown to the user.
 * @param out   filled in on success.
 * @return false for a parameter with no numeric wire form (THEREMINI_TEXT).
 *
 * @note @c out->count can be 1 even for a 14-bit parameter: at the very ends of
 *       the range the reference implementation sends the high controller alone.
 *       See "Decisions already forced by the data" in DESIGN.md.
 */
bool theremini_value_export(const theremini_param *param, double value,
                            theremini_wire *out);

#ifdef __cplusplus
}
#endif

#endif /* THEREMINI_PROTOCOL_H */
