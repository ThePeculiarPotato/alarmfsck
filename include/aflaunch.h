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
#include <chrono>
extern "C" {
#include <libtar.h>
}

class AfSystemException;

class AlarmFsckLauncher : public Gtk::Window
{
public:
    // typedef to avoid typing
    using af_time_point = typename std::chrono::time_point<std::chrono::system_clock>;

    AlarmFsckLauncher();
    void check_good_to_go();
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
    af_time_point timeStart, timeWakeup;
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

    // event handlers
    void on_in_at_combo_box_change();
    void on_hostage_check_box_click();
    void on_hostage_select_button_click();
    void on_ok_button_click();

    // GUI helper funcs
    void populate_in_at_combo_box();
    void populate_time_unit_combo_box();

    // filesystem related
    void check_hostage_file();
    void write_hostage_list_file();
    void write_or_update_hostage_list_file();
    void write_hostage_archive();
    void add_path_to_archive(TAR*,const std::string&);
    void write_compressed_hostage_archive();
    void erase_original_hostages();

    // time related
    bool check_time_entry();
    bool check_interval_entry();
    bool check_datetime_entry();
    void perform_rtc_check();

    void error_to_user(const Glib::ustring&, const std::string&);
    void error_to_user(const AfSystemException&);
    void error_to_user(const std::string&);
};

std::string format_time_point(const std::tm& timePoint_tm, const std::string& format);
std::string format_time_point(const AlarmFsckLauncher::af_time_point&, const std::string& format);
std::tm time_point_to_tm(const AlarmFsckLauncher::af_time_point& timePoint);

#endif // ORG_WALRUS_ALARMFSCK_AFLAUNCH_H
