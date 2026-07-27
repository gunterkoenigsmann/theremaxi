/* ALSA sequencer transport. A C reworking of the MIDI handling in the perl
 * Device.pm: open a client with an input and an output port, find the Theremini
 * by name and connect, send sysex, and read a sysex reply while passing the
 * antenna control-change stream to a callback.
 *
 * Copyright (C) 2017 Peter Niebling and contributors. GPL-3.0-or-later.
 */

#define _GNU_SOURCE 1 /* ALSA headers need the GNU feature set under -std=c11 */

#include "theremini/alsa.h"
#include "theremini/device.h"

#include <alsa/asoundlib.h>

#include <poll.h>
#include <stdlib.h>
#include <string.h>

/* a sequencer client needs only a couple of poll descriptors */
#define MAX_PFD 8

struct theremini_alsa {
	snd_seq_t *seq;
	int in_port;
	int out_port;

	bool connected;
	int dev_client;
	int dev_port;
	char dev_name[64];

	theremini_cc_fn on_cc;
	void *cc_user;

	theremini_channel_probe *active_probe; /* fed during channel detection */
};

/* dispatch one control-change event to whoever is listening */
static void handle_cc(theremini_alsa *self, const snd_seq_event_t *ev)
{
	const int channel = ev->data.control.channel;
	const int param = ev->data.control.param;
	if (self->active_probe) {
		theremini_channel_probe_feed(self->active_probe, channel, param);
	}
	if (self->on_cc) {
		self->on_cc(channel, param, ev->data.control.value, self->cc_user);
	}
}

theremini_alsa *theremini_alsa_open(const char *client_name)
{
	theremini_alsa *self = calloc(1, sizeof *self);
	if (!self) {
		return NULL;
	}
	self->connected = false;
	self->dev_client = -1;

	if (snd_seq_open(&self->seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
		free(self);
		return NULL;
	}
	snd_seq_set_client_name(self->seq, client_name ? client_name : "ThereMaxi");

	self->in_port = snd_seq_create_simple_port(
		self->seq, "in",
		SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
		SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
	self->out_port = snd_seq_create_simple_port(
		self->seq, "out",
		SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ,
		SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);

	if (self->in_port < 0 || self->out_port < 0) {
		theremini_alsa_close(self);
		return NULL;
	}
	return self;
}

void theremini_alsa_close(theremini_alsa *self)
{
	if (!self) {
		return;
	}
	if (self->seq) {
		snd_seq_close(self->seq);
	}
	free(self);
}

const char *theremini_alsa_discover(theremini_alsa *self)
{
	snd_seq_client_info_t *cinfo;
	snd_seq_port_info_t *pinfo;
	snd_seq_client_info_alloca(&cinfo);
	snd_seq_port_info_alloca(&pinfo);

	self->connected = false;
	self->dev_client = -1;

	snd_seq_client_info_set_client(cinfo, -1);
	while (snd_seq_query_next_client(self->seq, cinfo) >= 0) {
		const int client = snd_seq_client_info_get_client(cinfo);
		const char *name = snd_seq_client_info_get_name(cinfo);
		if (!theremini_client_matches(name)) {
			continue;
		}

		/* find a port that both reads and writes MIDI */
		snd_seq_port_info_set_client(pinfo, client);
		snd_seq_port_info_set_port(pinfo, -1);
		while (snd_seq_query_next_port(self->seq, pinfo) >= 0) {
			const unsigned caps = snd_seq_port_info_get_capability(pinfo);
			if ((caps & SND_SEQ_PORT_CAP_READ) && (caps & SND_SEQ_PORT_CAP_WRITE)) {
				self->dev_client = client;
				self->dev_port = snd_seq_port_info_get_port(pinfo);
				snprintf(self->dev_name, sizeof self->dev_name, "%s", name);
				break;
			}
		}
		if (self->dev_client >= 0) {
			break;
		}
	}

	if (self->dev_client < 0) {
		return NULL;
	}

	/* our in <- device out, our out -> device in */
	if (snd_seq_connect_from(self->seq, self->in_port, self->dev_client, self->dev_port) < 0 ||
	    snd_seq_connect_to(self->seq, self->out_port, self->dev_client, self->dev_port) < 0) {
		return NULL;
	}

	self->connected = true;
	return self->dev_name;
}

bool theremini_alsa_connected(const theremini_alsa *self)
{
	return self->connected;
}

bool theremini_alsa_send(theremini_alsa *self, const uint8_t *data, size_t len)
{
	snd_seq_event_t ev;
	snd_seq_ev_clear(&ev);
	snd_seq_ev_set_source(&ev, self->out_port);
	snd_seq_ev_set_subs(&ev);
	snd_seq_ev_set_direct(&ev);
	snd_seq_ev_set_sysex(&ev, len, (void *)data);

	if (snd_seq_event_output(self->seq, &ev) < 0) {
		return false;
	}
	return snd_seq_drain_output(self->seq) >= 0;
}

void theremini_alsa_on_cc(theremini_alsa *self, theremini_cc_fn cb, void *user)
{
	self->on_cc = cb;
	self->cc_user = user;
}

int theremini_alsa_pump(theremini_alsa *self, int timeout_ms)
{
	int npfd = snd_seq_poll_descriptors_count(self->seq, POLLIN);
	if (npfd > MAX_PFD) {
		npfd = MAX_PFD;
	}
	struct pollfd pfd[MAX_PFD];
	snd_seq_poll_descriptors(self->seq, pfd, (unsigned)npfd, POLLIN);

	const int ready = poll(pfd, (nfds_t)npfd, timeout_ms);
	if (ready <= 0) {
		return ready; /* 0 timeout, negative error */
	}

	int count = 0;
	snd_seq_event_t *ev = NULL;
	while (snd_seq_event_input(self->seq, &ev) >= 0 && ev) {
		if (ev->type == SND_SEQ_EVENT_CONTROLLER) {
			handle_cc(self, ev);
		}
		count++;
		if (snd_seq_event_input_pending(self->seq, 0) == 0) {
			break;
		}
	}
	return count;
}

int theremini_alsa_detect_channel(theremini_alsa *self, int timeout_ms)
{
	theremini_channel_probe probe;
	theremini_channel_probe_init(&probe);
	self->active_probe = &probe;

	const int slice = 50;
	int remaining = timeout_ms;
	int result = -1;
	while (remaining > 0) {
		theremini_alsa_pump(self, slice < remaining ? slice : remaining);
		result = theremini_channel_probe_result(&probe);
		if (result >= 0) {
			break;
		}
		remaining -= slice;
	}

	self->active_probe = NULL;
	return result;
}

long theremini_alsa_read_sysex(theremini_alsa *self, uint8_t *buf, size_t cap,
                               int timeout_ms)
{
	int npfd = snd_seq_poll_descriptors_count(self->seq, POLLIN);
	if (npfd > MAX_PFD) {
		npfd = MAX_PFD;
	}
	struct pollfd pfd[MAX_PFD];

	size_t got = 0; /* bytes accumulated into buf */

	for (;;) {
		snd_seq_poll_descriptors(self->seq, pfd, (unsigned)npfd, POLLIN);
		const int ready = poll(pfd, (nfds_t)npfd, timeout_ms);
		if (ready < 0) {
			return -1;
		}
		if (ready == 0) {
			return 0; /* timeout */
		}

		snd_seq_event_t *ev = NULL;
		while (snd_seq_event_input(self->seq, &ev) >= 0 && ev) {
			if (ev->type == SND_SEQ_EVENT_SYSEX) {
				const uint8_t *chunk = ev->data.ext.ptr;
				const size_t n = ev->data.ext.len;
				if (got + n > cap) {
					return -2; /* would overflow */
				}
				memcpy(buf + got, chunk, n);
				got += n;
				if (n > 0 && chunk[n - 1] == 0xf7) {
					return (long)got; /* a complete message */
				}
			} else if (ev->type == SND_SEQ_EVENT_CONTROLLER) {
				handle_cc(self, ev);
			}

			if (snd_seq_event_input_pending(self->seq, 0) == 0) {
				break;
			}
		}
	}
}
