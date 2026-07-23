/* Internal accessors for the generated message templates. Not part of the
 * public API - message.c uses these to reach the byte arrays that the generated
 * tables.c holds.
 *
 * Copyright (C) 2017 Peter Niebling and contributors. GPL-3.0-or-later.
 */

#ifndef THEREMINI_INTERNAL_H
#define THEREMINI_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

const uint8_t *theremini_tmpl_identity_request(size_t *len);
const uint8_t *theremini_tmpl_request_all_presets(size_t *len);
const uint8_t *theremini_tmpl_preset_name_prefix(size_t *len);
const uint8_t *theremini_tmpl_preset_name_suffix(size_t *len);
const uint8_t *theremini_tmpl_effect_name_prefix(size_t *len);
const uint8_t *theremini_tmpl_effect_name_suffix(size_t *len);

#endif
