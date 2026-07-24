// The main window: a preset list beside a notebook of parameter pages. Opening
// a .theremaxi library fills the list; selecting a preset loads it into the
// editor; Store writes the editor back to the preset; Save writes the library.
//
// Copyright (C) 2017 Peter Niebling and contributors. GPL-3.0-or-later.

#ifndef THEREMAXI_MAINFRAME_H
#define THEREMAXI_MAINFRAME_H

#include "library.hpp"

#include <wx/frame.h>

#include <vector>

class ParamControl;
class wxListBox;
class wxNotebook;

class MainFrame : public wxFrame {
public:
	MainFrame();

	// Load a library at startup (from the command line) and show its first
	// preset. Errors go to the status bar rather than a dialog.
	void LoadLibrary(const wxString &path);

private:
	void BuildPages(wxNotebook *book);
	void OnOpen(wxCommandEvent &);
	void OnSaveAs(wxCommandEvent &);
	void OnNewLibrary(wxCommandEvent &);
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
	wxListBox *m_presetList = nullptr;

	theremaxi::Library m_library;
	int m_current = -1;
};

#endif
