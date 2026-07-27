/**
 * @file alsa.h
 * @brief The ALSA MIDI transport for the Theremini - the Linux-only layer that
 *        opens sequencer ports, finds the device and moves bytes.
 *
 * This is the part of the device library that needs the hardware. It builds on
 * the portable core in device.h (device-name matching, input reassembly) and on
 * the message building and decoding in the protocol library. Everything here is
 * a thin wrapper over ALSA's sequencer, kept separate so the rest stays testable
 * without a device.
 *
 * Copyright (C) 2017 Peter Niebling and contributors. GPL-3.0-or-later.
 */

#ifndef THEREMINI_ALSA_H
#define THEREMINI_ALSA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** An open connection to the ALSA sequencer. Opaque. */
typedef struct theremini_alsa theremini_alsa;

/** Called for each incoming control-change message - the antenna stream. */
typedef void (*theremini_cc_fn)(int channel, int cc, int value, void *user);

/**
 * @brief Open a sequencer client with an input and an output port.
 * @param client_name the name this client registers under.
 * @return a handle, or NULL on failure.
 */
theremini_alsa *theremini_alsa_open(const char *client_name);

/** @brief Close the connection and free the handle (NULL is ignored). */
void theremini_alsa_close(theremini_alsa *seq);

/**
 * @brief Find a Theremini among the sequencer's clients and connect to it.
 * @param seq the connection.
 * @return the device's client name (owned by @p seq) on success, else NULL.
 */
const char *theremini_alsa_discover(theremini_alsa *seq);

/** @brief Whether a device is currently connected. */
bool theremini_alsa_connected(const theremini_alsa *seq);

/**
 * @brief Send raw bytes to the device - a full F0..F7 sysex or a short message.
 * @return true on success.
 */
bool theremini_alsa_send(theremini_alsa *seq, const uint8_t *data, size_t len);

/**
 * @brief Wait for one complete sysex reply.
 *
 * Incoming control-change messages (the antennas) are handed to the callback
 * set with theremini_alsa_on_cc, if any, and otherwise dropped, so the antenna
 * stream does not get in the way of reading a reply.
 *
 * @param seq        the connection.
 * @param buf        receives the message including its F0 and F7.
 * @param cap        size of @p buf.
 * @param timeout_ms how long to wait, in milliseconds.
 * @return the message length, 0 on timeout, or negative on error/overflow.
 */
long theremini_alsa_read_sysex(theremini_alsa *seq, uint8_t *buf, size_t cap,
                               int timeout_ms);

/** @brief Set the callback for incoming control-change messages. */
void theremini_alsa_on_cc(theremini_alsa *seq, theremini_cc_fn cb, void *user);

/**
 * @brief Listen to the antenna stream and report the channel it uses.
 *
 * Watches incoming control-change messages for up to @p timeout_ms and returns
 * the channel the antennas stream on, so the input can be configured without
 * asking the user. Messages are still passed to the CC callback meanwhile.
 *
 * @return the channel 0-15, or -1 if no antenna traffic was seen in time.
 */
int theremini_alsa_detect_channel(theremini_alsa *seq, int timeout_ms);

/**
 * @brief Process one batch of incoming events, without waiting for a sysex.
 *
 * Waits up to @p timeout_ms for events, dispatches the control-change ones to
 * the callback, and returns. This is what an event loop uses to keep up with
 * the antenna stream; theremini_alsa_read_sysex is for a request/reply.
 *
 * @return the number of events processed, 0 on timeout, negative on error.
 */
int theremini_alsa_pump(theremini_alsa *seq, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* THEREMINI_ALSA_H */
