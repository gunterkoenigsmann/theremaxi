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

typedef struct {
	const char *name;    /* which builder */
	const char *arg;     /* the name argument, or NULL for the constant messages */
	const uint8_t *bytes;
	size_t size;
} golden_control;

extern const golden_control golden_controls[];
extern const size_t golden_control_count;

typedef struct {
	int channel, cc, value; /* the incoming message */
	int has_emit;           /* whether a value was produced */
	int emit;               /* the produced value, if has_emit */
} golden_input_event;

typedef struct {
	const char *name;
	int channel, cc, wide; /* the antenna config */
	const golden_input_event *events;
	size_t event_count;
} golden_input;

extern const golden_input golden_inputs[];
extern const size_t golden_input_count;

#endif
