#include "alarmfuck.h"
#include "common.h"
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

    // Signal handlers
    m_button.signal_clicked().connect(sigc::mem_fun(*this,
		&AlarmFuck::on_button_clicked));
    // pressing enter in the answer field
    answerField.signal_activate().connect(sigc::mem_fun(*this,
		&AlarmFuck::on_button_clicked));
    // prevent closing if user hasn't entered correct answer
    signal_delete_event().connect(sigc::mem_fun(*this,
		&AlarmFuck::on_window_delete));

    // Random Stuff
    /* TODO: figure out a good combination of random numbers such that the
       user won't reach for a calculator. For example, such that the maximal
       result can be some value like 8000. Perhaps allow it to be set by user.
       Also use stdlibc++ Random. */
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

    // TODO: move data files into predefined user/system dirs
    std::string execDir;
    try{execDir = afCommon::get_executable_dir();}
    catch(AfSystemException& error){
	error_to_user(error.get_message() + ": Using current path.", error.what());
	execDir = "";
    }
    try{baseDir = afCommon::cpp_realpath(execDir + "../");}
    catch(AfSystemException& error){
	error_to_user(error.get_message() + ": Using ../", error.what());
	baseDir  = "../";
    }
    std::string audioPath = baseDir + data_dir + audio_file;
    std::string compressedPath = baseDir + data_dir + hostage_compressed;

    // Initializes the audio bits
    ca_context_create(&context);
    ca_context_set_driver(context, "alsa");
    ca_proplist_create(&props);
    ca_proplist_sets(props, CA_PROP_MEDIA_FILENAME, audioPath.c_str());

    // TODO: encrypt the hostage archive
    hasHostages = (access(compressedPath.c_str(), F_OK) == 0);

    //start the horrible loop
    workerThread =
	Glib::Threads::Thread::create(sigc::bind(sigc::mem_fun(worker,
			&LoopPlayWorker::loopPlay),
		    this));
}

// A bunch of error-message functions
void AlarmFuck::error_to_user(const std::string& appErrMessage, const std::string& sysErrMessage){
    // sysErrMessage is supposed to come from errno or a similar error mechanism
    if(sysErrMessage.size() > 0)
	std::cerr << sysErrMessage << "\n\t" << appErrMessage;
    else
	std::cerr << "\t" << appErrMessage;
    commentField.set_text(appErrMessage);
}

void AlarmFuck::error_to_user(const AfSystemException& error){
    error_to_user(error.get_message(), error.what());
}

void AlarmFuck::error_to_user(const std::string& appErrMessage){
    error_to_user(appErrMessage, "");
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
	try {
	    decompress_hostage_archive();
	    std::string fullHostageArchivePath = baseDir + data_dir + hostage_archive;
	    free_hostages(fullHostageArchivePath);
	    // erase the uncompressed archive
	    afCommon::erase_file(fullHostageArchivePath);
	} catch (AfSystemException& error) {error_to_user(error);}
    }
}

void AlarmFuck::free_hostages(const std::string& fullHostageArchivePath){
    // extract the archive
    // check for errors opening - the TAR_GNU option is necessary for files with long names
    TAR *tarStrucPtr = new TAR;
    if(tar_open(&tarStrucPtr, fullHostageArchivePath.c_str(), NULL, O_RDONLY, 0755, TAR_GNU) == -1){
	throw AfSystemException("Error opening " + hostage_archive);
	return;
    }
    char pathPrefix[] = "";
    if(tar_extract_all(tarStrucPtr, pathPrefix) == -1){
	throw AfSystemException("Error extracting archive");
	return;
    }
    tar_close(tarStrucPtr);
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
