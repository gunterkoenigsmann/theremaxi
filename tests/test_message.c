/* The device control messages, checked against the bytes the perl produced. */

#include "golden.h"
#include "theremini/protocol.h"

#include <stdio.h>
#include <string.h>

static int failures;

static size_t build(const golden_control *g, uint8_t *out, size_t cap)
{
	if (strcmp(g->name, "identity_request") == 0) {
		return theremini_msg_identity_request(out, cap);
	}
	if (strcmp(g->name, "request_all_presets") == 0) {
		return theremini_msg_request_all_presets(out, cap);
	}
	if (strcmp(g->name, "write_preset_name") == 0) {
		return theremini_msg_write_preset_name(g->arg, out, cap);
	}
	if (strcmp(g->name, "write_effect_name") == 0) {
		return theremini_msg_write_effect_name(g->arg, out, cap);
	}
	return 0;
}

static void check_against_golden(void)
{
	for (size_t i = 0; i < golden_control_count; i++) {
		const golden_control *g = &golden_controls[i];
		uint8_t out[THEREMINI_MESSAGE_MAX];

		const size_t len = build(g, out, sizeof out);

		if (len != g->size || memcmp(out, g->bytes, g->size) != 0) {
			printf("FAIL %s(%s): got", g->name, g->arg ? g->arg : "");
			for (size_t b = 0; b < len; b++) {
				printf(" %02x", out[b]);
			}
			printf(", want");
			for (size_t b = 0; b < g->size; b++) {
				printf(" %02x", g->bytes[b]);
			}
			printf("\n");
			failures++;
		}
	}
	printf("%zu control messages\n", golden_control_count);
}

/* A buffer too small must be refused, not overrun. */
static void check_capacity(void)
{
	uint8_t tiny[4];
	if (theremini_msg_request_all_presets(tiny, sizeof tiny) != 0) {
		printf("FAIL: a message was written into too small a buffer\n");
		failures++;
	}
	if (theremini_msg_write_preset_name("X", tiny, sizeof tiny) != 0) {
		printf("FAIL: a name message was written into too small a buffer\n");
		failures++;
	}
	printf("undersized buffers refused\n");
}

/* Encoding rules, spelled out rather than only implied by the framed messages. */
static void check_name_encode(void)
{
	uint8_t out[THEREMINI_NAME_BYTES];

	theremini_name_encode("", out);
	for (int i = 0; i < THEREMINI_NAME_BYTES; i++) {
		if (out[i] != ' ') {
			printf("FAIL: empty name is not all spaces\n");
			failures++;
			break;
		}
	}

	theremini_name_encode("  hi  ", out);
	if (memcmp(out, "hi           ", THEREMINI_NAME_BYTES) != 0) {
		printf("FAIL: '  hi  ' not trimmed and padded\n");
		failures++;
	}

	theremini_name_encode("ABCDEFGHIJKLMNOP", out); /* 16, over the limit */
	if (memcmp(out, "ABCDEFGHIJKLM", THEREMINI_NAME_BYTES) != 0) {
		printf("FAIL: an over-long name was not cut to 13\n");
		failures++;
	}

	theremini_name_encode(NULL, out);
	if (out[0] != ' ') {
		printf("FAIL: NULL name is not all spaces\n");
		failures++;
	}
	printf("name encoding: trim, pad, cut, NULL\n");
}

int main(void)
{
	check_against_golden();
	check_capacity();
	check_name_encode();

	if (failures) {
		printf("\n%d check(s) failed\n", failures);
		return 1;
	}
	printf("\nall checks passed\n");
	return 0;
}
