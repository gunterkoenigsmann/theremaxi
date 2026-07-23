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

#endif
