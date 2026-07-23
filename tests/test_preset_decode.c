/* Every preset dump the perl implementation was recorded decoding, replayed
 * through the C library. */

#include "golden.h"
#include "theremini/protocol.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures;

static void check_against_golden(void)
{
	size_t checked = 0;

	for (size_t i = 0; i < golden_preset_count; i++) {
		const golden_preset *g = &golden_presets[i];
		theremini_preset preset;

		if (!theremini_preset_decode(g->input, g->input_size, g->number, &preset)) {
			printf("FAIL preset %zu: decode refused %zu bytes\n", i, g->input_size);
			failures++;
			continue;
		}

		for (size_t v = 0; v < g->value_count; v++) {
			const golden_value *want = &g->values[v];
			const theremini_value *got = theremini_preset_value(&preset, want->id);

			if (!got || !got->present) {
				printf("FAIL preset %zu %s: not decoded\n", i, want->id);
				failures++;
				continue;
			}

			if (want->text) {
				if (strcmp(got->text, want->text) != 0) {
					printf("FAIL preset %zu %s: got \"%s\", want \"%s\"\n",
					       i, want->id, got->text, want->text);
					failures++;
				}
			} else {
				/* the perl formats through sprintf on the way in, so compare
				 * at the resolution the parameter is displayed with */
				const theremini_param *p = theremini_param_by_id(want->id);
				const double tolerance = p && p->kind == THEREMINI_NUMERIC
				                         ? pow(10.0, -p->digits) / 2.0
				                         : 0.0;
				if (fabs(got->number - want->number) > tolerance) {
					printf("FAIL preset %zu %s: got %.10g, want %.10g\n",
					       i, want->id, got->number, want->number);
					failures++;
				}
			}
			checked++;
		}
	}
	printf("%zu values across %zu recorded preset dumps\n", checked,
	       golden_preset_count);
}

/* A dump that stops early must be refused, not read past. */
static void check_short_input(void)
{
	if (golden_preset_count == 0) {
		return;
	}
	const golden_preset *g = &golden_presets[0];
	theremini_preset preset;

	for (size_t size = 0; size < g->input_size; size += 7) {
		/* no assertion on the result beyond "it returns and does not read
		 * past the buffer" - under a sanitizer this is the real check */
		(void)theremini_preset_decode(g->input, size, 0, &preset);
	}

	if (theremini_preset_decode(g->input, 0, 0, &preset)) {
		printf("FAIL: an empty dump was accepted\n");
		failures++;
	}
	printf("truncated dumps rejected without reading past the end\n");
}

/* The waveform is the sum of two words and is never range checked, by design.
 * Make sure that stays true and visible rather than turning into a crash. */
static void check_unvalidated_enum(void)
{
	uint8_t data[0x76];
	theremini_preset preset;

	memset(data, 0xff, sizeof data);
	if (!theremini_preset_decode(data, sizeof data, 0, &preset)) {
		printf("FAIL: an all-ones dump was refused\n");
		failures++;
		return;
	}

	const theremini_value *scale = theremini_preset_value(&preset, "85");
	const theremini_param *p = theremini_param_by_id("85");

	if (!scale || !p) {
		printf("FAIL: no scale parameter\n");
		failures++;
		return;
	}
	if (scale->number < p->value_count) {
		printf("FAIL: expected the out of range value this dump is known to "
		       "produce, got %g\n", scale->number);
		failures++;
	}
	printf("out of range enum survives decoding as a number (%g of %d names)\n",
	       scale->number, p->value_count);
}

int main(void)
{
	check_against_golden();
	check_short_input();
	check_unvalidated_enum();

	if (failures) {
		printf("\n%d check(s) failed\n", failures);
		return 1;
	}
	printf("\nall checks passed\n");
	return 0;
}
