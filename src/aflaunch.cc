#include "aflaunch.h"
#include <gtkmm/application.h>
#include <gtkmm/messagedialog.h>
#include <glibmm.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <exception>
extern "C" {
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <libgen.h>
#include <sys/types.h>
}
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>

#define PADDING 10
#define DATA_DIR "data/"
#define BIN_DIR "bin/"
#define HOSTAGE_FILE "hostages.af"
#define HOSTAGE_ARCHIVE "hostages.tar"
#define HOSTAGE_COMPRESSED "hostages.tar.gz"
#define HIB_EXEC "hibernator"
#define FILE_DELIM '/'
#define SUGGESTED_HOURS 8

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
	fileChooserWindow(*this),
	pathHashList(fileChooserWindow),
	updatedFileList(false)
{
	fileChooserWindow.set_hash_list(pathHashList);
	set_border_width(PADDING);
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

	//progressBar.set_size_request(-1,20);
	progressBar.set_show_text(true);

	hostageSelectButton.set_sensitive(false);
	hostageSelectButton.signal_clicked().connect(sigc::mem_fun(*this,
				&AlarmFuckLauncher::on_hostage_select_button_click));
	hostageCheckBox.signal_toggled().connect(sigc::mem_fun(*this,
				&AlarmFuckLauncher::on_hostage_check_box_click));
	okButton.signal_clicked().connect(sigc::mem_fun(*this,
				&AlarmFuckLauncher::on_ok_button_click));
	cancelButton.signal_clicked().connect(sigc::mem_fun(*this,
				&Gtk::Widget::hide));

	timeIntervalEntry.set_text(std::to_string(SUGGESTED_HOURS));
	timeIntervalEntry.set_alignment(1);
	timeIntervalEntry.set_width_chars(4);

	inHBox.pack_start(timeIntervalEntry, Gtk::PACK_EXPAND_PADDING);
	inHBox.pack_start(timeUnitComboBox, Gtk::PACK_EXPAND_WIDGET);

	timeStart = Glib::DateTime::create_now_local();
	timeWakeup = timeStart.add_hours(SUGGESTED_HOURS);
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
	inAtComboBox.signal_changed().connect(sigc::mem_fun(*this,
				&AlarmFuckLauncher::on_in_at_combo_box_change));

	show_all_children();
	progressBar.hide();

	/* Currently the data dir is ../data relative to the executable's path.
	 * Later it will search in appropriate subdirectories of /usr/share and
	 * ~.
	 * The temporary ../data approach requires finding the executable path.
	 * This is done non-portably through a system call and /proc/self/exe.
	 * In case of error, the search is done relative to the current
	 * directory. */
	char exePath[PATH_MAX];
	ssize_t exePathSize = readlink("/proc/self/exe", exePath, PATH_MAX);
	if(exePathSize != -1){
		exePath[exePathSize] = '\0';
		baseDir = std::string(dirname(exePath)) + "/../";
		// resolve subdirectories and such
		char *pathCand = realpath(baseDir.c_str(), NULL);
		if(pathCand == NULL){
			error_to_user("Could not canonicalize base path, using ../");
			baseDir = "../";
		} else {
			baseDir = std::string(pathCand) + FILE_DELIM;
			std::cout << baseDir << std::endl;
		}
	} else baseDir = "../";

	fullHostageFilePath = baseDir + DATA_DIR + HOSTAGE_FILE;
	std::cout << fullHostageFilePath << std::endl;
	if(access(fullHostageFilePath.c_str(), F_OK) == 0){
		std::cout << "Found hostage file " << HOSTAGE_FILE << std::endl;
		if(pathHashList.import_file(fullHostageFilePath.c_str())){
			update_user_hash_list();
			hostageCheckBox.set_active();
			hostageCheckBox.toggled();
		}
	}

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
	okButton.set_sensitive( !takingHostages || !userPathHashList.empty());
	if(takingHostages && !userPathHashList.empty()){
		progressBar.show();
		char* buf = new char[10];
		humanize_number(buf, 7, userPathHashList.get_size(), "B", HN_AUTOSCALE, 0);
		progressBar.set_text("Ready to scramble " + std::string(buf) + " of hostages.");
	}
	else progressBar.hide();
}

void AlarmFuckLauncher::error_to_stdout(std::string preString){
	std::cout << preString << ": " << strerror(errno) << std::endl;
}

void AlarmFuckLauncher::error_to_user(std::string stdoutPreString, Glib::ustring progBarString){
	error_to_stdout(stdoutPreString);
	progressBar.set_text(progBarString);
}

void AlarmFuckLauncher::error_to_user(std::string preString){
	error_to_user(preString, preString + ".");
}


bool AlarmFuckLauncher::write_hostage_list_file(){
	progressBar.set_text("Writing hostage list to disk.");
	std::ofstream ofs(fullHostageFilePath);
	// check for errors
	if(!ofs){
		error_to_user("Could not open " HOSTAGE_FILE);
		return false;
	}
	// loop over path_map and write out top paths
	std::unordered_map<std::string,PathHashEntry>& path_map = userPathHashList.pathHashList;
	for(auto it = path_map.begin(); it != path_map.end(); it++){
		ofs << it->first << std::endl;
	}
	ofs.close();
	return true;
}

bool AlarmFuckLauncher::add_path_to_archive(TAR *tarStrucPtr, std::string str){
	const char *fpath = str.c_str();
	if( tar_append_file(tarStrucPtr, fpath, fpath) == -1){
		error_to_user("Error adding " + str + " to archive");
		progressBar.set_fraction(0.0);
		tar_close(tarStrucPtr);
		return false;
	}
	return true;
}

bool AlarmFuckLauncher::write_hostage_archive(){
	double currentSize = 0, totalSize = userPathHashList.get_size();
	TAR *tarStrucPtr = new TAR;
	// check for errors opening - the TAR_GNU option is necessary for files with long names
	std::string fullHostageArchivePath = baseDir + DATA_DIR + HOSTAGE_ARCHIVE;
	if( tar_open(&tarStrucPtr, fullHostageArchivePath.c_str(), NULL, O_WRONLY | O_CREAT | O_TRUNC, 0755, TAR_GNU) == -1){
		error_to_user("Error opening " HOSTAGE_ARCHIVE);
		return false;
	}
	std::unordered_map<std::string,PathHashEntry>& path_map = userPathHashList.pathHashList;
	// double loop over path_map
	for(auto it = path_map.begin(); it != path_map.end(); it++){
		// ordinary file
		if(it->second.subfilesPointer == NULL){
			if(!add_path_to_archive(tarStrucPtr,it->first)) return false;
		}
		// directory
		else{
			std::unordered_set<std::string>& subPaths = *(it->second.subfilesPointer);
			for(auto it2 = subPaths.begin(); it2 != subPaths.end(); it2++){
				if(!add_path_to_archive(tarStrucPtr,*it2)) return false;
			}
		}
		// update progress bar TODO: save individual file sizes as well
		// to allow finer updating
		currentSize += it->second.totalSize;
		progressBar.set_fraction(currentSize/totalSize);
	}
	// finalize archive
	if( tar_append_eof(tarStrucPtr) == -1){
		error_to_user("Error finalizing " HOSTAGE_ARCHIVE);
		progressBar.set_fraction(0.0);
		tar_close(tarStrucPtr);
		return false;
	}
	// close and finish
	tar_close(tarStrucPtr);
	return true;
}

bool AlarmFuckLauncher::check_time_entry(){
	Gtk::TreeModel::iterator iter = inAtComboBox.get_active();
	switch((*iter)[inAtColumnRecord.idCol]){
	case 1:{
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
		break;}
	case 2:{
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
		break;}
	}
	return true;
}

void AlarmFuckLauncher::on_ok_button_click(){
	
	if(!check_time_entry()) return;
	
	Gtk::MessageDialog dialog(*this, "Are you sure?", false,
			Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_OK_CANCEL);
	dialog.set_secondary_text("Just a reminder on how very annoying this is\
 going to be. And don't forget to turn up your speakers if you still want to do this.");
	if(dialog.run() != Gtk::RESPONSE_OK) return;
	
	// Write hostage file, don't update it if it wasn't modified after importing it
	if(!updatedFileList && access(fullHostageFilePath.c_str(), F_OK) == 0){
		struct stat * statBuf = new struct stat;
		if(stat(fullHostageFilePath.c_str(), statBuf) != 0){
			error_to_user("Could not open " HOSTAGE_FILE);
			return;
		}
		if(statBuf->st_mtime > timeStart.to_unix()){
			if(!write_hostage_list_file()) return;
		}
	} else {
		if(!write_hostage_list_file()) return;
	}

	// write everything to tar archive
	if(!write_hostage_archive()) return;

	if(!write_compressed_hostage_archive()) return;

	// TODO: safely erase all the previously existing files
	if(!perform_rtc_check()) return;

	Glib::DateTime finalTimeCheck = Glib::DateTime::create_now_local();
	finalTimeCheck.add_seconds(5);

	if(timeWakeup.compare(finalTimeCheck) <= 0){
		error_to_user("You should really set this a bit further in the future.");
		return;
	}
	
	std::vector<std::string> args({baseDir + BIN_DIR + HIB_EXEC,std::to_string(timeWakeup.to_unix())});
	Glib::spawn_async("",args);

	std::cout << "exiting" << std::endl;

	hide();
}

bool AlarmFuckLauncher::write_compressed_hostage_archive(){
	// TODO: add error checking
	namespace io = boost::iostreams;
	std::ofstream fileOut(baseDir + DATA_DIR + HOSTAGE_COMPRESSED, std::ios_base::out | std::ios_base::binary);
	io::filtering_streambuf<io::output> out;
	out.push(io::gzip_compressor());
	out.push(fileOut);
	std::ifstream fileIn(baseDir + DATA_DIR + HOSTAGE_ARCHIVE, std::ios_base::in | std::ios_base::binary);
	io::copy(fileIn, out);
	out.pop();
	fileIn.close();
	return true;
}

#define DEFAULT_RTC			"rtc0"
#define DEFAULT_RTC_DEVICE		"/dev/" DEFAULT_RTC
#define SYS_WAKEUP_PATH			"/sys/class/rtc/" DEFAULT_RTC "/device/power/wakeup"
#define SYS_POWER_STATE_PATH		"/sys/power/state"

bool AlarmFuckLauncher::perform_rtc_check(){
	if(access(DEFAULT_RTC_DEVICE, F_OK)){
		error_to_user("Could not find RTC device");
		return false;
	}
	std::ifstream ifs(SYS_POWER_STATE_PATH);
	if(!ifs){
		error_to_user("Could not determine hibernate capability");
		return false;
	}
	std::string tempString;
	std::getline(ifs, tempString);
	ifs.close();
	if(!ifs){
		error_to_user("Error closing power state file");
		return false;
	}

	if(tempString.find("disk") == std::string::npos){
		error_to_user("Sorry, system does not support hibernation");
		return false;
	}

	ifs.open(SYS_WAKEUP_PATH);
	if(!ifs){
		error_to_user("Sorry, wakeups not enabled");
		return false;
	}
	std::getline(ifs, tempString);
	if(tempString.compare("enabled") != 0){
		error_to_user("Sorry, wakeups not enabled");
		return false;
	}
	return true;
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

