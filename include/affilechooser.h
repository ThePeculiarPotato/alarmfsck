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
#include <gtkmm/treestore.h>
#include <unordered_map>

class AlarmFuckLauncher;

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
    Glib::RefPtr<Gtk::TreeStore> filenameTreeStore;

    // for faster lookups
    std::unordered_map<std::string, Gtk::TreeStore::iterator> hashMap;

    // Parent window
    AlarmFuckLauncher& parentWindow;

    off_t totalSize;
    void check_path_prerequisites(const std::string&);
    std::pair<bool, off_t> is_accessible_directory(const std::string&);
    void insert_checked_entry(const std::string&, off_t);
    int traversal_func(const char*, const struct stat*, int);
public:
    explicit AlarmFuckFileChooser(AlarmFuckLauncher& parent);
    void notify(const std::string&);
    void populate_path_hash_view(std::vector<std::string>);
    bool check_and_add_path(const std::string&);
    void erase(Gtk::TreeIter&);
};

#endif /* end of include guard: ORG_WALRUS_ALARMFUCK_AFFCHOOSER_H */
