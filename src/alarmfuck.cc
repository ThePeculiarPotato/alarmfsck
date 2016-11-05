#include "alarmfuck.h"
#include "consts.h"
#include <iostream>
#include <fstream>
#include <canberra-gtk.h>
#include <gtkmm/application.h>
#include <glibmm/random.h>
#include <glibmm/timer.h>
extern "C" {
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <libtar.h>
#include <libgen.h>
#include <sys/types.h>
}
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>

#define CONTEXT_ID 1

AlarmFuck::AlarmFuck()
: m_button("Fuck Off!"), context(NULL), props(NULL), dispatcher(), worker(), workerThread(0), vBox(Gtk::ORIENTATION_VERTICAL, 5)
{
	set_border_width(10);
	set_title("Poor Bastard");

	// Signal handler
	m_button.signal_clicked().connect(sigc::mem_fun(*this,
	      &AlarmFuck::on_button_clicked));

	answerField.signal_activate().connect(sigc::mem_fun(*this,
	      &AlarmFuck::on_button_clicked));

	signal_delete_event().connect(sigc::mem_fun(*this,
	      &AlarmFuck::on_window_delete));

	// Random Stuff
	Glib::Rand randomGen;
	randNo1 = randomGen.get_int_range(1, 199);
	randNo2 = randomGen.get_int_range(1, 199);
	std::stringstream ss;
	ss << randNo1 << " * " << randNo2;

	// Pack widgets
	add(vBox);
	questionField.set_text(ss.str());
	answerField.set_max_length(50);
	answerField.set_alignment(.5);
	answerField.set_activates_default(true);
	vBox.pack_start(questionField, Gtk::PACK_SHRINK);
	vBox.pack_start(answerField, Gtk::PACK_SHRINK);
	vBox.pack_start(commentField, Gtk::PACK_SHRINK);
	vBox.pack_start(m_button, Gtk::PACK_SHRINK);

	show_all_children();

	// TODO: put this in a function

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
			// TODO: implement error handling
			std::cout << "Could not canonicalize base path, using ../" << std::endl;
			baseDir = "../";
		} else {
			baseDir = std::string(pathCand) + file_delim;
			// DEBUG
			std::cout << baseDir << std::endl;
		}
	} else baseDir = "../";

	std::string audioPath = baseDir + data_dir + audio_file;

	// Initializes the audio bits
	ca_context_create(&context);
	ca_context_set_driver(context, "alsa");
	ca_proplist_create(&props);
	ca_proplist_sets(props, CA_PROP_MEDIA_FILENAME, audioPath.c_str());

	// TODO: encrypt the hostage archive
	hasHostages = (access(std::string(baseDir + data_dir + hostage_compressed).c_str(), F_OK) == 0);
	// DEBUG
	std::cout << hasHostages << std::endl;

	//start the horrible loop
	workerThread =
		Glib::Threads::Thread::create(sigc::bind(sigc::mem_fun(worker,
						&LoopPlayWorker::loopPlay),
					this));
}

AlarmFuck::~AlarmFuck()
{
}

void playbackFinishedCallback(ca_context* c, uint32_t id, int error_code, void *mutexHolder)
{
	// Some horrible pointer notation. Synchronizes and sets the loop
	// finished indicator to signal the worker thread to initiate another
	// loop.
	Glib::Threads::Mutex::Lock lock(*(((LoopPlayWorker*)mutexHolder)->getMutex()));
	((LoopPlayWorker*)mutexHolder)->signalLoopFinish();
}

void AlarmFuck::play_sound(LoopPlayWorker* mutexHolder)
{
	int errorCode = ca_context_play_full(context, CONTEXT_ID, props, &playbackFinishedCallback, mutexHolder);
	if (errorCode < 0) puts( ca_strerror( errorCode ) );
}


int parseAnswerString(std::string holly)
{
	std::stringstream s;
	int result;

	s << holly;
	s >> result;

	return result;
}

void AlarmFuck::on_button_clicked()
{
	if(parseAnswerString(answerField.get_text().raw()) != randNo1*randNo2)
	{
		commentField.set_text("Wrooong!!!");
		return;
	}
	commentField.set_text("Whoa, you did it.");
	// Cancel the playback
	Glib::Threads::Mutex::Lock lock(*(worker.getMutex()));
	bool hasFinishedLoop = worker.hasFinishedLoop();
	worker.signalStopPlayback();
	lock.release();
	if(!hasFinishedLoop){
		int errorCode = ca_context_cancel(context, CONTEXT_ID);
		if (errorCode < 0) puts( ca_strerror( errorCode ) );
	}
	if(hasHostages){
		decompress_hostage_archive();
		// extract the archive
		// check for errors opening - the TAR_GNU option is necessary for files with long names
		TAR *tarStrucPtr = new TAR;
		std::string fullHostageArchivePath = baseDir + data_dir + hostage_archive;
		if( tar_open(&tarStrucPtr, fullHostageArchivePath.c_str(), NULL, O_RDONLY, 0755, TAR_GNU) == -1){
			error_to_user("Error opening " + hostage_archive);
			return;
		}
		char someString[] = "";
		if(tar_extract_all(tarStrucPtr, someString) == -1){
			error_to_user("Error extracting archive");
			return;
		}
		// erase the uncompressed archive
		erase_file(fullHostageArchivePath);
	}
}

bool AlarmFuck::erase_file(std::string str){
	// TODO: write settings to a file and come up with a static function
	// that takes a settings structure.
	if(std::remove(str.c_str()) == -1){
		error_to_user("Error removing " + str);
		return false;
	}
	return true;
}

bool AlarmFuck::decompress_hostage_archive(){
	// TODO: add error checking
	namespace io = boost::iostreams;
	std::ifstream fileIn(baseDir + data_dir + hostage_compressed, std::ios_base::in | std::ios_base::binary);
	io::filtering_streambuf<io::input> in;
	in.push(io::gzip_decompressor());
	in.push(fileIn);
	std::ofstream fileOut(baseDir + data_dir + hostage_archive, std::ios_base::out | std::ios_base::binary);
	io::copy(in, fileOut);
	in.pop();
	fileOut.close();
	return true;
}

void AlarmFuck::error_to_user(std::string mess){
	std::cout << mess << ": " << strerror(errno) << std::endl;
}

bool AlarmFuck::on_window_delete(GdkEventAny* event)
{
	if(parseAnswerString(answerField.get_text().raw()) != randNo1*randNo2)
	{
		commentField.set_text("Not so fast!!!");
		return true;
	}
	return on_delete_event(event);
}

void LoopPlayWorker::loopPlay(AlarmFuck* caller)
{
	Glib::Threads::Mutex::Lock lock(*getMutex(), Glib::Threads::NOT_LOCK);
	// keep looping and playing the sample until someone presses the button
	while(true)
	{
		lock.acquire();
		if(stop_playback){
			lock.release();
			break;
		}
		finished_loop = false;
		lock.release();
		caller->play_sound(this);
		// only play the sample again once it's finished, as signalled
		// by the canberra callback method
		while(true){
			Glib::usleep(100000);
			lock.acquire();
			if(finished_loop || stop_playback){
				lock.release();
				break;
			}
			lock.release();
		}
	}
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

	AlarmFuck alarmFuck;

	//Shows the window and returns when it is closed.
	int ret = app->run(alarmFuck);
	std::cout << "Exit code: " << ret << std::endl;
	return ret;
}
