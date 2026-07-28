/* The device library's pure core: antenna input reassembly replayed from the
 * perl, and device-name matching. */

#include "golden.h"
#include "theremini/device.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int failures;

static void ok(bool cond, const char *what)
{
	if (!cond) {
		printf("FAIL - %s\n", what);
		failures++;
	}
}

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

static void check_feedback(void)
{
	// 7-bit antenna, map 0..127 onto 0..100, gate [10,120], sensitivity 5
	theremini_feedback fb = {
		.enabled = true, .low = 10, .high = 120, .sens = 5, .revert = false,
		.out_min = 0, .out_max = 100, .input_max = 0x7f, .last = 0,
	};
	double out = -1;

	ok(!theremini_feedback_feed(&fb, 40, &out), "first value only primes 'last'");
	ok(theremini_feedback_feed(&fb, 60, &out) && out > 46 && out < 48,
	   "a big move emits: 60/127*100 ~= 47");
	ok(!theremini_feedback_feed(&fb, 62, &out), "a small move (2 < 5) is ignored");
	ok(theremini_feedback_feed(&fb, 90, &out) && out > 70 && out < 72,
	   "another big move emits: 90/127*100 ~= 71");
	ok(!theremini_feedback_feed(&fb, 5, &out), "below the gate is ignored");
	ok(!theremini_feedback_feed(&fb, 200, &out), "above the gate is ignored");

	// disabled row never emits
	fb.enabled = false;
	ok(!theremini_feedback_feed(&fb, 60, &out), "a disabled row is silent");

	// revert maps the antenna the other way
	theremini_feedback rev = {
		.enabled = true, .low = 0, .high = 127, .sens = 1, .revert = true,
		.out_min = 0, .out_max = 100, .input_max = 0x7f, .last = 0,
	};
	theremini_feedback_feed(&rev, 10, &out); // prime
	ok(theremini_feedback_feed(&rev, 100, &out) && out > 20 && out < 22,
	   "revert: value 100 maps near (127-100)/127*100 ~= 21");
	printf("feedback: gate, sensitivity, scale, revert, enable\n");
}

int main(void)
{
	check_input();
	check_discovery();
	check_channel_probe();
	check_feedback();

	if (failures) {
		printf("\n%d check(s) failed\n", failures);
		return 1;
	}
	printf("\nall checks passed\n");
	return 0;
}
