/**
 * @file device.h
 * @brief The device-facing pieces that do not need the hardware: reassembling an
 *        antenna's MIDI input, and recognising the device by name.
 *
 * The ALSA transport that actually opens ports and moves bytes is a separate,
 * Linux-only layer (not yet written); everything here is pure and portable, so
 * it is tested against the reference implementation without a Theremini.
 *
 * Copyright (C) 2017 Peter Niebling and contributors. GPL-3.0-or-later.
 */

#ifndef THEREMINI_DEVICE_H
#define THEREMINI_DEVICE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** The controllers the antennas stream on (fixed; the channel is configurable). */
#define THEREMINI_VOLUME_CC 2
#define THEREMINI_PITCH_CC 20

/** How one antenna's MIDI input arrives. */
typedef struct {
	int channel; /**< MIDI channel, 0-15 */
	int cc;      /**< controller number carrying the value (or its high bits) */
	bool wide;   /**< 14-bit: the value is on @c cc (high) and @c cc + 32 (low) */
} theremini_input_config;

/**
 * Guesses which MIDI channel the antennas use by watching the stream.
 *
 * The device streams its antennas continuously as control-change messages on
 * the two antenna controllers, so counting which channel carries them reveals
 * the configured channel without asking the user.
 */
typedef struct {
	int hits[16]; /**< antenna-controller messages seen on each channel */
} theremini_channel_probe;

/** @brief Reset a channel probe. */
void theremini_channel_probe_init(theremini_channel_probe *probe);

/** @brief Note one incoming control-change message. */
void theremini_channel_probe_feed(theremini_channel_probe *probe, int channel, int cc);

/**
 * @brief The channel the antennas appear to use.
 * @return the channel 0-15, or -1 if too little antenna traffic was seen to tell.
 */
int theremini_channel_probe_result(const theremini_channel_probe *probe);

/** Reassembles one antenna's input. Zero-initialise via theremini_input_init. */
typedef struct {
	theremini_input_config config;
	int pending; /**< held high bits, or -1 when none */
} theremini_input;

/** @brief Set up an input reassembler for the given configuration. */
void theremini_input_init(theremini_input *in, theremini_input_config config);

/**
 * @brief Feed one incoming control-change message.
 *
 * In 7-bit mode a value on the configured controller passes straight through.
 * In 14-bit mode the configured controller supplies the high seven bits and
 * controller+32 the low seven, so a value only appears once both have arrived.
 *
 * @param in      the reassembler.
 * @param channel the message's MIDI channel.
 * @param cc      the message's controller number.
 * @param value   the message's value, 0-127.
 * @param out     receives the reassembled value when the function returns true.
 * @return true if a value was produced.
 */
bool theremini_input_feed(theremini_input *in, int channel, int cc, int value, int *out);

/**
 * @brief Whether an ALSA client name is a Theremini.
 *
 * A case-insensitive search for "theremini" anywhere in the name, matching how
 * the reference implementation discovers the device.
 *
 * @param name the client name, or NULL.
 * @return true if it names a Theremini.
 */
bool theremini_client_matches(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* THEREMINI_DEVICE_H */
