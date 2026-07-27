/* The device library's pure core: antenna input reassembly replayed from the
 * perl, and device-name matching. */

#include "golden.h"
#include "theremini/device.h"

#include <stdio.h>
#include <string.h>

static int failures;

// Replay each recorded input sequence and check the values it produces.
static void check_input(void)
{
	size_t checked = 0;

	for (size_t i = 0; i < golden_input_count; i++) {
		const golden_input *g = &golden_inputs[i];
		theremini_input in;
		theremini_input_init(&in,
		                     (theremini_input_config){ g->channel, g->cc, g->wide != 0 });

		for (size_t e = 0; e < g->event_count; e++) {
			const golden_input_event *ev = &g->events[e];
			int out = 0;
			const bool emitted = theremini_input_feed(&in, ev->channel, ev->cc,
			                                          ev->value, &out);

			if (emitted != (ev->has_emit != 0) || (emitted && out != ev->emit)) {
				printf("FAIL %s step %zu: in(%d,%d,%d) gave %s%d, want %s%d\n",
				       g->name, e, ev->channel, ev->cc, ev->value,
				       emitted ? "emit " : "no-emit ", emitted ? out : 0,
				       ev->has_emit ? "emit " : "no-emit ", ev->emit);
				failures++;
			}
			checked++;
		}
	}
	printf("%zu input steps across %zu sequences\n", checked, golden_input_count);
}

static void check_matches(const char *name, bool want)
{
	if (theremini_client_matches(name) != want) {
		printf("FAIL: match(%s) != %d\n", name ? name : "(null)", want);
		failures++;
	}
}

static void check_discovery(void)
{
	check_matches("Moog Theremini", true);
	check_matches("theremini", true);
	check_matches("THEREMINI 24:0", true);
	check_matches("some Theremini port", true);
	check_matches("Theremin", false); // the classic, not the Theremini
	check_matches("piano", false);
	check_matches("", false);
	check_matches(NULL, false);
	printf("device-name matching\n");
}

static void check_channel_probe(void)
{
	theremini_channel_probe p;

	// nothing seen -> unknown
	theremini_channel_probe_init(&p);
	if (theremini_channel_probe_result(&p) != -1) {
		printf("FAIL: empty probe should be -1\n");
		failures++;
	}

	// a stream of antenna controllers on channel 0, plus noise elsewhere
	theremini_channel_probe_init(&p);
	for (int i = 0; i < 20; i++) {
		theremini_channel_probe_feed(&p, 0, THEREMINI_VOLUME_CC);
		theremini_channel_probe_feed(&p, 0, THEREMINI_PITCH_CC);
	}
	theremini_channel_probe_feed(&p, 5, 7);  // unrelated controller, ignored
	theremini_channel_probe_feed(&p, 9, THEREMINI_VOLUME_CC); // a stray, below threshold
	if (theremini_channel_probe_result(&p) != 0) {
		printf("FAIL: probe should find channel 0, got %d\n",
		       theremini_channel_probe_result(&p));
		failures++;
	}

	// too little traffic to be sure
	theremini_channel_probe_init(&p);
	theremini_channel_probe_feed(&p, 3, THEREMINI_PITCH_CC);
	if (theremini_channel_probe_result(&p) != -1) {
		printf("FAIL: a single hit should not be trusted\n");
		failures++;
	}
	printf("channel probe: detect, ignore noise, and stay unsure on too little\n");
}

int main(void)
{
	check_input();
	check_discovery();
	check_channel_probe();

	if (failures) {
		printf("\n%d check(s) failed\n", failures);
		return 1;
	}
	printf("\nall checks passed\n");
	return 0;
}
