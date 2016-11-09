#ifndef ORG_WALRUS_ALARMFSCK_AFFCHOOSER_H
#define ORG_WALRUS_ALARMFSCK_AFFCHOOSER_H

#include <gtkmm/window.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <gtkmm/box.h>
#include <gtkmm/buttonbox.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/treeview.h>
#include <gtkmm/textview.h>
#include <gtkmm/treestore.h>
#include <sigc++/functors/slot.h>
#include <unordered_map>
//#define _XOPEN_SOURCE = 666
extern "C" {
#include <ftw.h>
}

class AlarmFsckLauncher;

class AlarmFsckFileChooser : public Gtk::Window
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
		    { add(nameCol); add(sizeCol); }
		    Gtk::TreeModelColumn<off_t> sizeCol;
		    Gtk::TreeModelColumn<std::string> nameCol;
    } fileViewColumnRecord;
    Glib::RefPtr<Gtk::TreeStore> filenameTreeStore;

    // for faster lookups
    std::unordered_map<std::string, Gtk::TreeStore::iterator> hashMap;

    // Parent window
    AlarmFsckLauncher& parentWindow;

    off_t totalSize;
    void check_path_prerequisites(const std::string&);
    std::pair<bool, off_t> is_accessible_directory(const std::string&);
    void insert_checked_entry(const std::string&, off_t, const bool is_child=false);
    void populate_row(const Gtk::TreeModel::iterator&, const std::string&, off_t);
    void move_subtree(const Gtk::TreeStore::iterator&, const Gtk::TreeStore::iterator&);
    void relocate_subtree(const std::string&);
    void erase_subtree(const Gtk::TreeStore::iterator&);
    bool updatedFileList;

    // DEBUG
    void print_tree();
    void print_subtree(int level, const Gtk::TreeModel::iterator&);

    // ugly hack to be able to use the C-based ftw (file tree walk) routine
    // TODO: think about security issues with multiple instances of this class floating around
    static int traversal_func(const char*, const struct stat*, int, struct FTW*);
    static AlarmFsckFileChooser * const get_set_first_obj_ptr(AlarmFsckFileChooser* currentObject){
	static AlarmFsckFileChooser * const eternalObject = currentObject;
	return eternalObject;
    }
public:
    explicit AlarmFsckFileChooser(AlarmFsckLauncher& parent);
    void notify(const std::string&);
    void populate_file_view(std::vector<std::string>);
    bool check_and_add_path(const std::string&);
    bool is_empty() const {return hashMap.empty();};
    bool is_updated() const {return updatedFileList;};
    void set_updated(bool updated_status) {updatedFileList = updated_status;};
    const std::unordered_map<std::string, Gtk::TreeStore::iterator>& get_hash_map() const {return hashMap;};
    long get_total_size() const {return totalSize;};
    const ModelColumns& get_column_record() const {return fileViewColumnRecord;};
    std::vector<std::string> get_top_paths() const;
    void for_each_file(const Gtk::TreeModel::SlotForeachIter& a) {filenameTreeStore->foreach_iter(a);};
    void import_file(const std::string&);
};

#endif /* end of include guard: ORG_WALRUS_ALARMFSCK_AFFCHOOSER_H */
