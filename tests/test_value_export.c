/* Every value-to-wire conversion the perl implementation was recorded doing,
 * replayed through the C library. A mismatch means the port changed behaviour.
 */

#include "golden.h"
#include "theremini/protocol.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void check_against_golden(void)
{
	for (size_t i = 0; i < golden_export_count; i++) {
		const golden_export *g = &golden_exports[i];
		const theremini_param *p = theremini_param_by_id(g->id);

		if (!p) {
			printf("FAIL %s: no such parameter\n", g->id);
			failures++;
			continue;
		}

		theremini_wire w;
		if (!theremini_value_export(p, g->value, &w)) {
			printf("FAIL %s: export refused value %g\n", g->id, g->value);
			failures++;
			continue;
		}

		if (w.count != g->wire_count ||
		    memcmp(w.bytes, g->wire, (size_t)g->wire_count) != 0) {
			printf("FAIL %s value %g: got", g->id, g->value);
			for (int b = 0; b < w.count; b++) {
				printf(" %u", w.bytes[b]);
			}
			printf(", want");
			for (int b = 0; b < g->wire_count; b++) {
				printf(" %u", g->wire[b]);
			}
			printf("\n");
			failures++;
		}
	}
	printf("%zu golden export vectors replayed\n", golden_export_count);
}

/* The wire domain is small enough to cover completely rather than sample. */
static void check_monotonic(void)
{
	size_t count;
	const theremini_param *params = theremini_params(&count);

	for (size_t i = 0; i < count; i++) {
		const theremini_param *p = &params[i];
		if (p->kind != THEREMINI_NUMERIC) {
			continue;
		}

		int previous = -1;
		const int steps = 4096;
		for (int s = 0; s <= steps; s++) {
			const double value = p->min + (p->max - p->min) * s / steps;
			theremini_wire w;
			if (!theremini_value_export(p, value, &w)) {
				printf("FAIL %s: export refused %g\n", p->id, value);
				failures++;
				break;
			}

			/* the shortcut at the top of the range sends one byte, so only
			 * compare the values that carry the full resolution */
			if (w.count != (p->bits == 14 ? 2 : 1)) {
				continue;
			}

			const int raw = p->bits == 14 ? (w.bytes[0] << 7 | w.bytes[1])
			                              : w.bytes[0];
			if (raw < previous) {
				printf("FAIL %s: not monotonic at %g (%d after %d)\n",
				       p->id, value, raw, previous);
				failures++;
				break;
			}
			previous = raw;
		}
	}
	printf("monotonicity checked over the full range of every numeric parameter\n");
}

static void check_table(void)
{
	size_t count;
	const theremini_param *params = theremini_params(&count);

	if (count != 27) {
		printf("FAIL: %zu parameters, expected 27\n", count);
		failures++;
	}

	for (size_t i = 0; i < count; i++) {
		const theremini_param *p = &params[i];

		if (theremini_param_by_id(p->id) != p) {
			printf("FAIL %s: lookup by id returns something else\n", p->id);
			failures++;
		}
		if (p->cc >= 0 && theremini_param_by_cc(p->cc) != p) {
			printf("FAIL %s: lookup by cc returns something else\n", p->id);
			failures++;
		}
		if (p->bits == 14 && p->lsb_cc != p->cc + 32) {
			printf("FAIL %s: 14 bit but lsb_cc is %d\n", p->id, p->lsb_cc);
			failures++;
		}
		if (p->kind == THEREMINI_ENUM && p->value_count <= 0) {
			printf("FAIL %s: enum without values\n", p->id);
			failures++;
		}
	}
	printf("%zu parameters in the table\n", count);
}

int main(void)
{
	check_table();
	check_against_golden();
	check_monotonic();

	if (failures) {
		printf("\n%d check(s) failed\n", failures);
		return 1;
	}
	printf("\nall checks passed\n");
	return 0;
}
