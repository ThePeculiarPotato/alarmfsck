#ifndef ORG_WALRUS_ALARMFUCK_AFFCHOOSER_H
#define ORG_WALRUS_ALARMFUCK_AFFCHOOSER_H

#include "pathhashlist.h"
#include <gtkmm/window.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <gtkmm/box.h>
#include <gtkmm/buttonbox.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/treeview.h>
#include <gtkmm/textview.h>
#include <gtkmm/liststore.h>

class AlarmFuckLauncher;

class AlarmFuckFileChooser : public Gtk::Window, Notifiable
{

private:
	// Signal handlers
	void on_add_button_clicked(const std::string&);
	void on_remove_button_clicked();
	inline void on_cancel_button_clicked(){hide();}
	void on_import_button_clicked();
	void on_done_button_clicked();

	// General widgets:
	Gtk::Button addFilesButton, addFolderButton, removeButton, importButton, doneButton, cancelButton;
	Gtk::Label wakeMeUpLabel, onLabel, takeHostagesLabel;
	Gtk::Box vBox;
	Gtk::ButtonBox bottomButtonHBox;
	Gtk::TreeView fileView;
	Gtk::ScrolledWindow fileScroll, infoScroll;
	Gtk::TextView infoTextView;
	Glib::RefPtr<Gtk::TextBuffer> infoTextBuffer;

	class ModelColumns : public Gtk::TreeModelColumnRecord{
		public:
			ModelColumns()
			{ add(typeCol); add(nameCol); }
			Gtk::TreeModelColumn<Glib::ustring> typeCol;
			Gtk::TreeModelColumn<off_t> sizeCol;
			Gtk::TreeModelColumn<std::string> nameCol;
	} fileViewColumnRecord;
	Glib::RefPtr<Gtk::ListStore> fileViewListStore;

	// Parent window
	AlarmFuckLauncher& parentWindow;
	PathHashList* pathHashList;

public:
	explicit AlarmFuckFileChooser(AlarmFuckLauncher& parent);
	void notify(const std::string&) override;
	Gtk::TreeModel::iterator insert_entry(const Glib::ustring&, off_t, const std::string&);
	void erase(Gtk::TreeIter&);
	void set_hash_list(PathHashList& phl);
};

#endif /* end of include guard: ORG_WALRUS_ALARMFUCK_AFFCHOOSER_H */
