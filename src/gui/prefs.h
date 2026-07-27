// Application settings and the dialog that edits them.
//
// For now this is the MIDI input configuration - which channel and controller
// each antenna arrives on, and whether it is 7- or 14-bit - the same thing the
// perl app keeps under device.midi_input. Persisted with wxConfig so it survives
// restarts, ready for when the GUI talks to the device.
//
// Copyright (C) 2017 Peter Niebling and contributors. GPL-3.0-or-later.

#ifndef THEREMAXI_PREFS_H
#define THEREMAXI_PREFS_H

#include <wx/dialog.h>

// One antenna's input. Defaults match a default-configured Theremini: the
// antennas stream on channel 0, volume on controller 2, pitch on controller 20.
struct AntennaInput {
	int channel;
	int cc;
	bool wide; // 14-bit
};

struct Settings {
	AntennaInput volume{0, 2, false};
	AntennaInput pitch{0, 20, false};
};

// wxConfig-backed load and save.
Settings load_settings();
void save_settings(const Settings &settings);

// Edits a Settings. ShowModal() returns wxID_OK if the user accepted; read the
// result with GetSettings().
class PrefsDialog : public wxDialog {
public:
	PrefsDialog(wxWindow *parent, const Settings &settings);
	Settings GetSettings() const;

private:
	struct Row;
	Row *m_volume;
	Row *m_pitch;
};

#endif
