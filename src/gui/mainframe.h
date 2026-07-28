// The main window: a preset list beside a notebook of parameter pages. Opening
// a .theremaxi library fills the list; selecting a preset loads it into the
// editor; Store writes the editor back to the preset; Save writes the library.
//
// Copyright (C) 2017 Peter Niebling and contributors. GPL-3.0-or-later.

#ifndef THEREMAXI_MAINFRAME_H
#define THEREMAXI_MAINFRAME_H

#include "library.hpp"
#include "prefs.h"

#include <wx/frame.h>

#include <vector>

class ParamControl;
class FeedbackTab;
class wxListBox;
class wxNotebook;

#ifdef THEREMINI_HAVE_ALSA
struct theremini_alsa; // opaque, from theremini/alsa.h
#include <wx/timer.h>
#endif

class MainFrame : public wxFrame {
public:
	MainFrame();
	~MainFrame() override;

	// Load a library at startup (from the command line) and show its first
	// preset. Errors go to the status bar rather than a dialog.
	void LoadLibrary(const wxString &path);

#ifdef THEREMINI_HAVE_ALSA
	// Called from the ALSA callback for each incoming antenna message.
	void OnAntenna(int channel, int cc, int value);
#endif

private:
	void BuildPages(wxNotebook *book);
	void OnOpen(wxCommandEvent &);
	void OnSaveAs(wxCommandEvent &);
	void OnNewLibrary(wxCommandEvent &);
	void OnPreferences(wxCommandEvent &);
	void OnSelectPreset(wxCommandEvent &);
	void OnStore(wxCommandEvent &);
	void OnNewPreset(wxCommandEvent &);
	void OnCopyPreset(wxCommandEvent &);
	void OnDeletePreset(wxCommandEvent &);

	void SelectPreset(int index); // update list, editor and m_current

	void RefreshPresetList();
	void ApplyPreset(const theremaxi::Preset &preset);
	theremaxi::Preset CollectPreset(int number) const;

	std::vector<ParamControl *> m_controls;
	FeedbackTab *m_feedback = nullptr;
	wxListBox *m_presetList = nullptr;

	theremaxi::Library m_library;
	int m_current = -1;
	Settings m_settings;

#ifdef THEREMINI_HAVE_ALSA
	void OnConnect(wxCommandEvent &);
	void OnDisconnect(wxCommandEvent &);
	void OnSyncDevice(wxCommandEvent &);
	void OnSendDevice(wxCommandEvent &);
	void OnAutoDetect(wxCommandEvent &);
	void OnPump(wxTimerEvent &);
	void UpdateDeviceMenu();

	// Open the sequencer, discover a Theremini and start the pump. Returns true
	// on success. When silent, a missing device leaves no status message - used
	// for the auto-connect at startup, which falls back to offline quietly.
	bool ConnectDevice(bool silent);

	// The status-bar fields to the right of the message area, kept latched so
	// both antennas stay visible instead of flickering between the two.
	void ShowAntenna(int field, const char *which, int channel, int value);

	theremini_alsa *m_seq = nullptr;
	wxTimer m_pump;
	int m_last_volume = -1;
	int m_last_pitch = -1;
#endif
};

#endif
