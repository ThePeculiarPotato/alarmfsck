#include "common.h"
#include "aflaunch.h"
#include <glibmm/regex.h>
#include <glibmm/spawn.h>
#include <gtkmm/messagedialog.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <gtkmm/application.h>
extern "C" {
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <libgen.h>
#include <sys/types.h>
#include <bsd/stdlib.h>
}

AlarmFuckLauncher::AlarmFuckLauncher() :
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
    set_title("alarmfuck Launcher");
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

    timeStart = Glib::DateTime::create_now_local();
    timeWakeup = timeStart.add_hours(suggested_hours);
    timeEntry.set_text(timeWakeup.format("%T"));
    timeEntry.set_alignment(.5);
    timeEntry.set_width_chars(8);

    dateEntry.set_text(timeWakeup.format("%d/%m/%Y"));
    dateEntry.set_alignment(.5);
    dateEntry.set_width_chars(8);

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
	    &AlarmFuckLauncher::on_hostage_select_button_click));
    hostageCheckBox.signal_toggled().connect(sigc::mem_fun(*this,
	    &AlarmFuckLauncher::on_hostage_check_box_click));
    okButton.signal_clicked().connect(sigc::mem_fun(*this,
	    &AlarmFuckLauncher::on_ok_button_click));
    cancelButton.signal_clicked().connect(sigc::mem_fun(*this,
	    &Gtk::Widget::hide));
    inAtComboBox.signal_changed().connect(sigc::mem_fun(*this,
	    &AlarmFuckLauncher::on_in_at_combo_box_change));
    /* }}} UI setup */

    // show all
    show_all_children();
    progressBar.hide();

    /* Currently the data dir is ../data relative to the executable's path.
     * Later it will search in appropriate subdirectories of /usr/share and
     * ~.
     * The temporary ../data approach requires finding the executable path.
     * This is done non-portably through a system call and /proc/self/exe.
     * In case of error, the search is done relative to the current
     * directory. */
    std::string execDir;
    try{execDir = get_executable_dir();}
    catch(AfSystemException& error){
	error_to_user(error.get_message() + ": Using current path.", error.what());
	execDir = "";
    }
    try{baseDir = cpp_realpath(execDir + "../");}
    catch(AfSystemException& error){
	error_to_user(error.get_message() + ": Using ../", error.what());
	baseDir  = "../";
    }
    fullHostageFilePath = baseDir + data_dir + hostage_file;
    check_hostage_file();
    fileChooser.set_updated(false);
}

void AlarmFuckLauncher::populate_in_at_combo_box()
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

void AlarmFuckLauncher::populate_time_unit_combo_box()
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

void AlarmFuckLauncher::on_in_at_combo_box_change()
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

void AlarmFuckLauncher::on_hostage_check_box_click(){
    hostageSelectButton.set_sensitive(hostageCheckBox.get_active());
    check_good_to_go();
}

void AlarmFuckLauncher::check_good_to_go(){
    bool takingHostages = hostageCheckBox.get_active();
    okButton.set_sensitive(!takingHostages || !fileChooser.is_empty());
    if(takingHostages && !fileChooser.is_empty()){
	progressBar.show();
	char* buf = new char[10];
	humanize_number(buf, 7, fileChooser.get_total_size(), "B", HN_AUTOSCALE, 0);
	progressBar.set_text("Ready to scramble " + std::string(buf) + " of hostages.");
    }
    else progressBar.hide();
}

void AlarmFuckLauncher::on_hostage_select_button_click(){fileChooser.show();};

// A bunch of error-message functions
void AlarmFuckLauncher::error_to_user(const Glib::ustring& appErrMessage, const std::string& sysErrMessage){
    // sysErrMessage is supposed to come from errno or a similar error mechanism
    if(sysErrMessage.size() > 0)
	std::cerr << sysErrMessage << "\n\t" << appErrMessage;
    else
	std::cerr << "\t" << appErrMessage;
    progressBar.set_text(appErrMessage);
}

void AlarmFuckLauncher::error_to_user(const AfSystemException& error){
    error_to_user(error.get_message(), error.what());
}

void AlarmFuckLauncher::error_to_user(const std::string& appErrMessage){
    error_to_user(appErrMessage, "");
}

bool AlarmFuckLauncher::check_time_entry(){
    // TODO refactor this a bit
    Gtk::TreeModel::iterator iter = inAtComboBox.get_active();
    switch((*iter)[inAtColumnRecord.idCol]){
	case 1:
	    {
		double interval;
		try{ interval = stod(timeIntervalEntry.get_text().raw()); }
		catch(std::exception& e){
		    progressBar.set_text("Invalid time interval.");
		    return false;
		}
		if(interval <= 0){
		    progressBar.set_text("Wakeup time must be in the future.");
		    return false;
		}
		int modifier;
		Gtk::TreeModel::iterator iter2 = timeUnitComboBox.get_active();
		switch((*iter2)[timeUnitColumnRecord.idCol]){
		    case 1:
			modifier = 1;
			break;
		    case 2:
			modifier = 60;
			break;
		    case 3:
			modifier = 3600;
			break;
		}
		Glib::DateTime timeNow = Glib::DateTime::create_now_local();
		timeWakeup = timeNow.add_seconds(interval*modifier);
		break;
	    }
	case 2:
	    {
		Glib::RefPtr<Glib::Regex> timeRegex = Glib::Regex::create("^(\\d{2}):(\\d{2}):(\\d{2})$");
		Glib::MatchInfo timeMatch;
		if(!timeRegex->match(timeEntry.get_text(), timeMatch)){
		    progressBar.set_text("Invalid time format.");
		    return false;
		}
		Glib::RefPtr<Glib::Regex> dateRegex = Glib::Regex::create("^(\\d{2})/(\\d{2})/(\\d{4})$");
		Glib::MatchInfo dateMatch;
		if(!dateRegex->match(dateEntry.get_text(), dateMatch)){
		    // TODO: add options for "today" and "tomorrow"
		    progressBar.set_text("Invalid date format.");
		    return false;
		}
		// glibmm was either buggy or too clever for me, had to resort
		// to glib here
		GDateTime *gPotentialTime = g_date_time_new_local(
			stoi(dateMatch.fetch(3).raw()),
			stoi(dateMatch.fetch(2).raw()),
			stoi(dateMatch.fetch(1).raw()),
			stoi(timeMatch.fetch(1).raw()),
			stoi(timeMatch.fetch(2).raw()),
			stod(timeMatch.fetch(3).raw()));
		if(gPotentialTime == NULL){
		    progressBar.set_text("Not a real date.");
		    return false;
		}
		Glib::DateTime potentialTime = Glib::wrap(gPotentialTime);
		Glib::DateTime timeTwoNow = Glib::DateTime::create_now_local();
		if( potentialTime.compare(timeTwoNow) <= 0 ){
		    progressBar.set_text("Wakeup time must be in the future.");
		    return false;
		}
		timeWakeup = potentialTime;
		break;
	    }
    }
    return true;
}

void AlarmFuckLauncher::on_ok_button_click(){
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
    } catch(AfSystemException& error){
	error_to_user(error);
    }

    // final time check
    Glib::DateTime finalTimeCheck = Glib::DateTime::create_now_local();
    finalTimeCheck.add_seconds(5);
    if(timeWakeup.compare(finalTimeCheck) <= 0){
	error_to_user("You should really set this a bit further in the future.");
	return;
    }

    // run hibernator and exit
    std::vector<std::string> args({baseDir + bin_dir + hib_exec,std::to_string(timeWakeup.to_unix())});
    Glib::spawn_async("",args);
    std::cout << "exiting" << std::endl;
    hide();
}

int main (int argc, char *argv[])
{
    // process executable name and add it to your identifier string
    std::string idString = argv[0];
    size_t lastDashLocation = idString.find_last_of("/");
    idString = idString.substr(lastDashLocation + 1);
    std::string fullId = "org.walrus.alarmfuck." + idString;

    std::cout << "Started application " << argv[0] << " with app id: " << fullId << std::endl;
    Glib::RefPtr<Gtk::Application> app = Gtk::Application::create(argc, argv, fullId);

    AlarmFuckLauncher afLauncher;

    //Shows the window and returns when it is closed.
    int ret = app->run(afLauncher);
    std::cout << "Exit code: " << ret << std::endl;
    return ret;
}

