/* The seven-bit unpacking and dump framing, replayed from what the perl
 * implementation was recorded doing. */

#include "golden.h"
#include "theremini/protocol.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures;

/* The core bit twiddling, group by group. */
static void check_unpack3(void)
{
	for (size_t i = 0; i < golden_sx_count; i++) {
		const golden_sx *g = &golden_sx_vectors[i];
		const uint16_t got = theremini_sysex_unpack3(g->in[0], g->in[1], g->in[2]);

		if (got != g->out) {
			printf("FAIL unpack3(%u,%u,%u): got %u, want %u\n",
			       g->in[0], g->in[1], g->in[2], got, g->out);
			failures++;
		}
	}
	printf("%zu seven-bit unpacking vectors\n", golden_sx_count);
}

/* A whole dump: framing, unpacking and per-preset decode together. */
static void check_messages(void)
{
	size_t checked = 0;

	for (size_t i = 0; i < golden_message_count; i++) {
		const golden_message *g = &golden_messages[i];
		theremini_dump dump;

		const theremini_sysex_status status =
			theremini_sysex_decode(g->input, g->input_size, &dump);

		if (status != THEREMINI_SYSEX_OK) {
			printf("FAIL message %zu: decode returned status %d\n", i, status);
			failures++;
			continue;
		}
		if (dump.count != g->preset_count) {
			printf("FAIL message %zu: decoded %zu presets, want %zu\n",
			       i, dump.count, g->preset_count);
			failures++;
			continue;
		}

		for (size_t p = 0; p < g->preset_count; p++) {
			const golden_message_preset *gp = &g->presets[p];
			for (size_t v = 0; v < gp->value_count; v++) {
				const golden_value *want = &gp->values[v];
				const theremini_value *got =
					theremini_preset_value(&dump.presets[p], want->id);

				if (!got || !got->present) {
					printf("FAIL message %zu preset %zu %s: not decoded\n",
					       i, p, want->id);
					failures++;
					continue;
				}

				const theremini_param *param = theremini_param_by_id(want->id);
				const double tolerance = param && param->kind == THEREMINI_NUMERIC
				                         ? pow(10.0, -param->digits) / 2.0
				                         : 0.0;
				if (fabs(got->number - want->number) > tolerance) {
					printf("FAIL message %zu preset %zu %s: got %.10g, want %.10g\n",
					       i, p, want->id, got->number, want->number);
					failures++;
				}
				checked++;
			}
		}
	}
	printf("%zu values across %zu framed dumps\n", checked, golden_message_count);
}

/* The error paths the perl handles by dying; here they are a status code. */
static void check_rejects(void)
{
	theremini_dump dump;
	uint8_t buf[64];

	memset(buf, 0, sizeof buf);
	buf[2] = 0x99; /* not a known layout */
	buf[3] = 0x01;
	if (theremini_sysex_decode(buf, sizeof buf, &dump) != THEREMINI_SYSEX_BAD_HEADER) {
		printf("FAIL: an unknown header was not rejected\n");
		failures++;
	}

	buf[2] = 0x05; /* a single preset, but far too short to fill one */
	if (theremini_sysex_decode(buf, sizeof buf, &dump) != THEREMINI_SYSEX_TRUNCATED) {
		printf("FAIL: a short single-preset dump was not rejected\n");
		failures++;
	}

	if (theremini_sysex_decode(buf, 4, &dump) != THEREMINI_SYSEX_TRUNCATED) {
		printf("FAIL: a dump shorter than its header was not rejected\n");
		failures++;
	}
	printf("malformed dumps rejected with a status instead of a crash\n");
}

int main(void)
{
	check_unpack3();
	check_messages();
	check_rejects();

	if (failures) {
		printf("\n%d check(s) failed\n", failures);
		return 1;
	}
	printf("\nall checks passed\n");
	return 0;
}
