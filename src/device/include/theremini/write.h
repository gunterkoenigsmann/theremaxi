/**
 * @file write.h
 * @brief Writing a preset to the device.
 *
 * Follows what the perl app does to save a preset: select the slot, send every
 * value as a control-change (and the names as sysex), then save. This changes
 * the device's stored presets, so take a backup first (theremini-probe --backup).
 *
 * Copyright (C) 2017 Peter Niebling and contributors. GPL-3.0-or-later.
 */

#ifndef THEREMINI_WRITE_H
#define THEREMINI_WRITE_H

#include "theremini/alsa.h"
#include "theremini/protocol.h"

#include <stdbool.h>

/**
 * @brief Write a preset into a device slot and save it.
 * @param seq     the connection.
 * @param channel the MIDI channel the device receives on (0 for a default device).
 * @param slot    the slot to save into, 0-based (slot 1 in the UI is 0 here).
 * @param preset  the values to write.
 * @return true if every message was sent.
 */
bool theremini_write_preset(theremini_alsa *seq, int channel, int slot,
                            const theremini_preset *preset);

#endif
