#include "common.h"
#include "aflaunch.h"
#include <glibmm/spawn.h>
#include <gtkmm/messagedialog.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <memory>
#include <gtkmm/application.h>
extern "C" {
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <libgen.h>
#include <sys/types.h>
}
#include <cryptopp/cryptlib.h>

AlarmFsckLauncher::AlarmFsckLauncher() :
    okButton("Go"),
    cancelButton("Cancel"),
    hostageSelectButton("Select Files"),
    wakeMeUpLabel("Wake me up "),
    onLabel("on"),
    takeHostagesLabel("Take Hostages"),
    topHBox(Gtk::ORIENTATION_HORIZONTAL, 5),
    hostageHBox(Gtk::ORIENTATION_HORIZONTAL, 5),
    bottomButtonHBox(Gtk::ORIENTATION_HORIZONTAL),
    inHBox(Gtk::ORIENTATION_HORIZONTAL, 5),
    atHBox(Gtk::ORIENTATION_HORIZONTAL, 5),
    vBox(Gtk::ORIENTATION_VERTICAL, 5),
    fileChooser(*this)
{

    /* UI setup {{{ */
    // window setup
    set_border_width(padding);
    set_title("alarmfsck Launcher");
    set_default_size(380, 150);

    // Widget setup
    topHBox.pack_start(wakeMeUpLabel, Gtk::PACK_EXPAND_WIDGET);
    topHBox.pack_start(inAtComboBox, Gtk::PACK_EXPAND_WIDGET);
    topHBox.pack_start(inHBox, Gtk::PACK_EXPAND_WIDGET);
    hostageHBox.pack_start(hostageCheckBox, Gtk::PACK_SHRINK);
    hostageHBox.pack_start(takeHostagesLabel, Gtk::PACK_SHRINK);
    hostageHBox.pack_start(hostageSelectButton, Gtk::PACK_SHRINK, 19);
    bottomButtonHBox.set_layout(Gtk::BUTTONBOX_SPREAD);
    bottomButtonHBox.set_spacing(40);
    bottomButtonHBox.add(okButton);
    bottomButtonHBox.add(cancelButton);
    vBox.pack_start(topHBox, Gtk::PACK_EXPAND_PADDING);
    vBox.pack_start(hostageHBox, Gtk::PACK_EXPAND_PADDING);
    vBox.pack_start(progressBar, Gtk::PACK_EXPAND_PADDING);
    vBox.pack_start(bottomButtonHBox, Gtk::PACK_EXPAND_PADDING);
    add(vBox);

    progressBar.set_show_text(true);

    timeIntervalEntry.set_text(std::to_string(suggested_hours));
    timeIntervalEntry.set_alignment(1);
    timeIntervalEntry.set_width_chars(4);

    inHBox.pack_start(timeIntervalEntry, Gtk::PACK_EXPAND_PADDING);
    inHBox.pack_start(timeUnitComboBox, Gtk::PACK_EXPAND_WIDGET);

    // time conversion stuff
    timeStart = std::chrono::system_clock::now();
    timeWakeup = timeStart + std::chrono::hours(suggested_hours);

    timeEntry.set_text(format_time_point(timeWakeup, "%T"));
    timeEntry.set_alignment(.5);
    timeEntry.set_width_chars(8);

    dateEntry.set_text(format_time_point(timeWakeup, "%d/%m/%Y"));
    dateEntry.set_alignment(.5);
    dateEntry.set_width_chars(10);

    atHBox.pack_start(timeEntry, Gtk::PACK_EXPAND_PADDING);
    atHBox.pack_start(onLabel, Gtk::PACK_EXPAND_WIDGET);
    atHBox.pack_start(dateEntry, Gtk::PACK_EXPAND_PADDING);
    atHBox.show_all_children();
    atHBox.show();

    // Drop-down menus
    populate_time_unit_combo_box();
    populate_in_at_combo_box();

    // associate signal handlers
    hostageSelectButton.set_sensitive(false);
    hostageSelectButton.signal_clicked().connect(sigc::mem_fun(*this,
	    &AlarmFsckLauncher::on_hostage_select_button_click));
    hostageCheckBox.signal_toggled().connect(sigc::mem_fun(*this,
	    &AlarmFsckLauncher::on_hostage_check_box_click));
    okButton.signal_clicked().connect(sigc::mem_fun(*this,
	    &AlarmFsckLauncher::on_ok_button_click));
    cancelButton.signal_clicked().connect(sigc::mem_fun(*this,
	    &Gtk::Widget::hide));
    inAtComboBox.signal_changed().connect(sigc::mem_fun(*this,
	    &AlarmFsckLauncher::on_in_at_combo_box_change));
    /* }}} UI setup */

    // show all
    show_all_children();
    progressBar.hide();

    // TODO: move data files into predefined user/system dirs
    try{binDir = afCommon::get_executable_dir();}
    catch(AfSystemException& error){
	error_to_user(error.get_message() + ": Using " + bin_dir, error.what());
	binDir = bin_dir;
    }

    // save intermediate files in $HOME/.alarmfsck/
    std::string homeDir = getenv("HOME");
    userDir = homeDir + "/." + package_name + "/";
    if(access(userDir.c_str(), W_OK | X_OK) == -1 && mkdir(userDir.c_str(), 0777) == -1){
	error_to_user("Could not access " + userDir + ": Using current path.");
	userDir = "";
    }

    hostageFilePath = userDir + hostage_file;
    archivePath = userDir + hostage_archive;
    compressedPath = userDir + hostage_compressed;

    check_hostage_file();
    fileChooser.set_updated(false);
}

void AlarmFsckLauncher::populate_in_at_combo_box()
{
    inAtListStore = Gtk::ListStore::create(inAtColumnRecord);
    inAtComboBox.set_model(inAtListStore);

    Gtk::TreeModel::Row row = *(inAtListStore->append());
    row[inAtColumnRecord.idCol] = 1;
    row[inAtColumnRecord.textCol] = "in";
    inAtComboBox.set_active(row);
    row = *(inAtListStore->append());
    row[inAtColumnRecord.idCol] = 2;
    row[inAtColumnRecord.textCol] = "at";
    inAtComboBox.pack_start(inAtColumnRecord.textCol);
}

void AlarmFsckLauncher::populate_time_unit_combo_box()
{
    timeUnitListStore = Gtk::ListStore::create(timeUnitColumnRecord);
    timeUnitComboBox.set_model(timeUnitListStore);

    Gtk::TreeModel::Row row = *(timeUnitListStore->append());
    row[timeUnitColumnRecord.idCol] = 1;
    row[timeUnitColumnRecord.textCol] = "seconds";
    row = *(timeUnitListStore->append());
    row[timeUnitColumnRecord.idCol] = 2;
    row[timeUnitColumnRecord.textCol] = "minutes";
    row = *(timeUnitListStore->append());
    row[timeUnitColumnRecord.idCol] = 3;
    row[timeUnitColumnRecord.textCol] = "hours";
    timeUnitComboBox.set_active(row);
    timeUnitComboBox.pack_start(timeUnitColumnRecord.textCol);
}

void AlarmFsckLauncher::on_in_at_combo_box_change()
{
    // kind of unsafe, cause there could be no active ones, but unlikely in
    // this app
    Gtk::TreeModel::Row row = *(inAtComboBox.get_active());
    if(row[inAtColumnRecord.idCol] == 1){
	topHBox.remove(atHBox);
	topHBox.pack_start(inHBox, Gtk::PACK_EXPAND_WIDGET);
    } else {
	topHBox.remove(inHBox);
	topHBox.pack_start(atHBox, Gtk::PACK_EXPAND_WIDGET);
    }
}

void AlarmFsckLauncher::on_hostage_check_box_click(){
    hostageSelectButton.set_sensitive(hostageCheckBox.get_active());
    check_good_to_go();
}

void AlarmFsckLauncher::check_good_to_go(){
    bool takingHostages = hostageCheckBox.get_active();
    okButton.set_sensitive(!takingHostages || !fileChooser.is_empty());
    if(takingHostages && !fileChooser.is_empty()){
	progressBar.show();
	progressBar.set_text("Ready to scramble "
		//+ afCommon::humanize_byte_count(fileChooser.get_total_size())
		+ afCommon::humanize_byte_count(fileChooser.get_total_size())
		+ " of hostages.");
    }
    else progressBar.hide();
}

void AlarmFsckLauncher::on_hostage_select_button_click(){fileChooser.show();};

// A bunch of error-message functions
void AlarmFsckLauncher::error_to_user(const Glib::ustring& appErrMessage, const std::string& sysErrMessage){
    // sysErrMessage is supposed to come from errno or a similar error mechanism
    if(sysErrMessage.size() > 0)
	std::cerr << appErrMessage << "\n\t" << sysErrMessage;
    else
	std::cerr << "\t" << appErrMessage << "\n";
    progressBar.set_text(appErrMessage);
}

void AlarmFsckLauncher::error_to_user(const AfSystemException& error){
    error_to_user(error.get_message(), error.what());
}

void AlarmFsckLauncher::error_to_user(const std::string& appErrMessage){
    error_to_user(appErrMessage, "");
}

void AlarmFsckLauncher::on_ok_button_click(){
    if(!check_time_entry()) return;

    Gtk::MessageDialog dialog(*this, "Are you sure?", false,
	    Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_OK_CANCEL);
    dialog.set_secondary_text("Just a reminder on how very annoying this is \
going to be. And don't forget to turn up your speakers if you still want to do this.");
    if(dialog.run() != Gtk::RESPONSE_OK) return;

    // perform all the checks
    try {
	write_or_update_hostage_list_file();
	write_hostage_archive();
	write_compressed_hostage_archive();
	erase_original_hostages();
	perform_rtc_check();
    } catch(const AfSystemException& error){
	error_to_user(error);
	return;
    } catch(const CryptoPP::Exception& error){
	error_to_user(error.what());
	return;
    }


    {
	using namespace std::chrono;
	// final time check
	if(timeWakeup <= (system_clock::now() + seconds(5))){
	    error_to_user("You should really set this a bit further in the future.");
	    return;
	}
	// run hibernator and exit
	// TODO: should I use a more specific cast?
	std::vector<std::string> args{binDir + hib_exec,
		std::to_string((unsigned long) system_clock::to_time_t(timeWakeup))};
	Glib::spawn_async("",args);
    }
    std::cout << "exiting" << std::endl;
    hide();
}

int main (int argc, char *argv[])
{
    // process executable name and add it to your identifier string
    std::string idString = argv[0];
    size_t lastDashLocation = idString.find_last_of("/");
    idString = idString.substr(lastDashLocation + 1);
    std::string fullId = "org.walrus.alarmfsck." + idString;

    std::cout << "Started application " << argv[0] << " with app id: " << fullId << std::endl;
    Glib::RefPtr<Gtk::Application> app = Gtk::Application::create(argc, argv, fullId);

    AlarmFsckLauncher afLauncher;

    //Shows the window and returns when it is closed.
    int ret = app->run(afLauncher);
    std::cout << "Exit code: " << ret << std::endl;
    return ret;
}

