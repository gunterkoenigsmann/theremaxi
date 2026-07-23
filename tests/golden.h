/* Shape of the generated fixture in golden_data.c. */

#ifndef THEREMINI_TEST_GOLDEN_H
#define THEREMINI_TEST_GOLDEN_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
	const char *id;     /* parameter, e.g. "74" */
	double value;       /* what the user sees */
	uint8_t wire[2];    /* what the perl implementation sends */
	int wire_count;
} golden_export;

extern const golden_export golden_exports[];
extern const size_t golden_export_count;

typedef struct {
	const char *id;
	double number;    /* used when text is NULL */
	const char *text;
} golden_value;

typedef struct {
	const uint8_t *input;
	size_t input_size;
	int number;             /* the slot it was decoded as */
	const golden_value *values;
	size_t value_count;
} golden_preset;

extern const golden_preset golden_presets[];
extern const size_t golden_preset_count;

typedef struct {
	uint8_t in[3];
	uint16_t out;
} golden_sx;

extern const golden_sx golden_sx_vectors[];
extern const size_t golden_sx_count;

typedef struct {
	const golden_value *values;
	size_t value_count;
} golden_message_preset;

typedef struct {
	const uint8_t *input;
	size_t input_size;
	const golden_message_preset *presets;
	size_t preset_count;
} golden_message;

extern const golden_message golden_messages[];
extern const size_t golden_message_count;

#endif
