#ifndef ORG_WALRUS_ALARMFSCK_AFLAUNCH_H
#define ORG_WALRUS_ALARMFSCK_AFLAUNCH_H

#include "affilechooser.h"
#include <gtkmm/button.h>
#include <gtkmm/window.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/label.h>
#include <gtkmm/box.h>
#include <gtkmm/buttonbox.h>
#include <gtkmm/combobox.h>
#include <gtkmm/entry.h>
#include <gtkmm/liststore.h>
#include <gtkmm/progressbar.h>
#include <glibmm/datetime.h>
extern "C" {
#include <libtar.h>
}

class AfSystemException;

class AlarmFsckLauncher : public Gtk::Window
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
    AlarmFsckFileChooser fileChooser;

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
    std::string prefixDir;
    std::string binDir;
    std::string hostageFilePath;
    std::string archivePath;
    std::string compressedPath;

    void on_in_at_combo_box_change();
    void on_hostage_check_box_click();
    void on_hostage_select_button_click();
    void on_ok_button_click();

    void populate_in_at_combo_box();
    void populate_time_unit_combo_box();

    void check_hostage_file();
    void write_hostage_list_file();
    void write_or_update_hostage_list_file();
    void write_hostage_archive();
    void add_path_to_archive(TAR*,const std::string&);
    bool check_time_entry();
    bool perform_rtc_check();
    void write_compressed_hostage_archive();
    void erase_original_hostages();

    void error_to_user(const Glib::ustring&, const std::string&);
    void error_to_user(const AfSystemException&);
    void error_to_user(const std::string&);
public:
    AlarmFsckLauncher();
    void check_good_to_go();
};

#endif // ORG_WALRUS_ALARMFSCK_AFLAUNCH_H
