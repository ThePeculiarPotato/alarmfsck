#ifndef ORG_WALRUS_ALARMFUCK_AFLAUNCH_H
#define ORG_WALRUS_ALARMFUCK_AFLAUNCH_H

#include <gtkmm/button.h>
#include <gtkmm/window.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/label.h>
#include <gtkmm/box.h>
#include <gtkmm/buttonbox.h>
#include <gtkmm/combobox.h>
#include <gtkmm/entry.h>
#include <gtkmm/liststore.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/textview.h>
#include <gtkmm/progressbar.h>
#include <glibmm/datetime.h>
#include <unordered_set>
#include <unordered_map>
#include <system_error>
extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libtar.h>
#include <ftw.h>
#include <bsd/stdlib.h>
}

class AlarmFuckLauncher;
class PathHashList;

class AlarmFuckFileChooser : public Gtk::Window
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
	void notify(const std::string&);
	Gtk::TreeModel::iterator insert_entry(const Glib::ustring&, off_t, const std::string&);
	void erase(Gtk::TreeIter&);
	void set_hash_list(PathHashList& phl);
};

class PathHashEntry{
public:
	PathHashEntry(){};
	PathHashEntry(std::unordered_set<std::string>* sp, off_t ts, Gtk::TreeModel::iterator tmr) : subfilesPointer(sp), totalSize(ts), rowIter(tmr){}
	PathHashEntry(std::unordered_set<std::string>* sp, off_t ts) : subfilesPointer(sp), totalSize(ts){}
	std::unordered_set<std::string>* subfilesPointer;
	off_t totalSize;
	Gtk::TreeModel::iterator rowIter;
};

class PathList
{
protected:
	off_t totalSize;
public:
	bool empty(){return pathHashList.empty();};
	off_t get_size(){return totalSize;};
	PathList& operator=(const PathList&);
	std::unordered_map<std::string, PathHashEntry> pathHashList;
};

// an unordered map for file keeping purposes. Keys are pathname strings and
// values are a struct of an unordered set pointer and an off_t. For paths
// representing files the pointer is null and the off_t is the file size in
// bytes. For directories, the pointer points to a list of all subfiles and the
// off_t is their total size.  If a file from an already included directory is
// added, it is ignored. If a directory containing already included files is
// added, those files' entries are deleted.
class PathHashList : public PathList
{
	AlarmFuckFileChooser& fileChooserWindow;
	struct stat *statBuf;
	PathHashEntry* currentTopEntry;
public:
	PathHashList(AlarmFuckFileChooser& fCW):
		fileChooserWindow(fCW)
	{
		totalSize = 0;
		statBuf = new struct stat;
		currentObject = this;
	};
	bool check_and_add_path(std::string);
	void remove_path(std::string);
	bool import_file(std::string);
	void populate_path_hash_view(std::vector<std::string>);

	static int traversal_func(const char *, const struct stat *, int);
	static PathHashList* currentObject;
};

class AlarmFuckLauncher : public Gtk::Window
{

private:
	// General widgets:
	Gtk::Button okButton, cancelButton, hostageSelectButton;
	Gtk::CheckButton hostageCheckBox;
	Gtk::Label wakeMeUpLabel, onLabel, takeHostagesLabel;
	Gtk::Box topHBox, vBox, inHBox, atHBox, hostageHBox;
	Gtk::ProgressBar progressBar;
	Gtk::ButtonBox bottomButtonHBox;
	Gtk::ComboBox inAtComboBox, timeUnitComboBox;
	Gtk::Entry timeIntervalEntry, timeEntry, dateEntry;
	Glib::DateTime timeStart, timeWakeup;
	AlarmFuckFileChooser fileChooserWindow;
	PathHashList pathHashList;
	PathList userPathHashList;
	bool updatedFileList;

	// Drop-down menus
	class ModelColumns : public Gtk::TreeModelColumnRecord{
		public:
			ModelColumns()
			{ add(idCol); add(textCol); }
			Gtk::TreeModelColumn<int> idCol;
			Gtk::TreeModelColumn<Glib::ustring> textCol;
	} inAtColumnRecord, timeUnitColumnRecord;
	Glib::RefPtr<Gtk::ListStore> inAtListStore;
	Glib::RefPtr<Gtk::ListStore> timeUnitListStore;

	// File paths
	std::string baseDir;
	std::string fullHostageFilePath;

	void on_in_at_combo_box_change();
	void on_hostage_check_box_click();
	void on_hostage_select_button_click(){fileChooserWindow.show();};
	void on_ok_button_click();

	void populate_in_at_combo_box();
	void populate_time_unit_combo_box();

	void check_hostage_file();
	bool write_hostage_list_file();
	bool write_hostage_archive();
	bool add_path_to_archive(TAR*,const std::string&);
	bool check_time_entry();
	bool perform_rtc_check();
	bool write_compressed_hostage_archive();
	bool erase_original_hostages();
	bool erase_file(const std::string&);

	void error_to_user(const Glib::ustring&, const std::string&, const bool =true);
	void error_to_user(const Glib::ustring&, const std::system_error&, const bool =true);
	void error_to_user(const Glib::ustring&, const bool =true);
public:
	AlarmFuckLauncher();
	void update_user_hash_list(){userPathHashList = pathHashList;};
	void check_good_to_go();
	void signal_updated_files(){ updatedFileList = true; }
};

#endif // ORG_WALRUS_ALARMFUCK_AFLAUNCH_H

