#include "alarmfsck.h"
#include "common.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <functional>
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
#include <cryptopp/filters.h>
#include <cryptopp/modes.h>
#include <cryptopp/files.h>

bool AlarmFsck::loopFinished;
bool AlarmFsck::stopPlayback;

const uint32_t canberra_context_id = 1;
const auto sleepDuration = std::chrono::milliseconds(100);
// this class implements some functionality involving two threads modifying a
// pair boolean indicator flags. Due to the predictable repeating sequence of
// checks and assignments these can be and are ordinary non-atomic bools.
// Should loopFinished evaluate to "false" in the looping thread after the
// callback thread already initiated setting it to "true", the user need only
// wait a short interval of length sleepDuration for the intended behaviour to
// resume.

AlarmFsck::AlarmFsck():
    m_button("Fsck Off!"),
    solvedIt(false),
    key(CryptoPP::AES::DEFAULT_KEYLENGTH),
    iv(CryptoPP::AES::BLOCKSIZE),
    vBox(Gtk::ORIENTATION_VERTICAL, 5)
{
    set_border_width(10);
    set_title("Poor Bastard");

    // Signal handlers
    m_button.signal_clicked().connect(sigc::mem_fun(*this,
		&AlarmFsck::on_button_clicked));
    // pressing enter in the answer field
    answerField.signal_activate().connect(sigc::mem_fun(*this,
		&AlarmFsck::on_button_clicked));
    // prevent closing if user hasn't entered correct answer
    signal_delete_event().connect(sigc::mem_fun(*this,
		&AlarmFsck::on_window_delete));

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
    std::string baseDir;
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
    prefixDir = baseDir + data_dir;
    compressedPath = prefixDir + hostage_compressed;
    encryptedPath = prefixDir + hostage_encrypted;
    std::string audioPath = prefixDir + audio_file;

    // Initializes the audio bits
    canberraContext = ca_gtk_context_get();
    // ca_context_set_driver(canberraContext, "alsa");
    ca_proplist_create(&canberraProps);
    ca_proplist_sets(canberraProps, CA_PROP_MEDIA_FILENAME, audioPath.c_str());

    hasHostages = (access(compressedPath.c_str(), F_OK) == 0);
    if(hasHostages){
	try {encrypt_hostage_archive();}
	catch (const CryptoPP::Exception& error) {
	    error_to_user(error.what());
	    hasHostages = false;
	}
	try {afCommon::erase_file(compressedPath);}
	catch (const AfSystemException& error){
	    error_to_user(error.get_message() + ":\nnow, more than ever, is the time to multiply!", error.what());
	}
    }

    //start the horrible loop
    loopFinished = true;
    stopPlayback = false;

    std::thread t(playback_looper, std::ref(canberraContext), std::ref(canberraProps));
    t.detach();
}

void AlarmFsck::playback_looper(ca_context* canCon, ca_proplist* canProp){
    while(true){
	if(loopFinished){
	    if(stopPlayback) break;
	    loopFinished = false;
	    ca_context_play_full(canCon, canberra_context_id, canProp,
		    &AlarmFsck::canberra_callback, nullptr);
	}
	std::this_thread::sleep_for(sleepDuration);
    }
}

void AlarmFsck::canberra_callback(ca_context *c, uint32_t id, int error_code, void *userdata){
    if(error_code != CA_SUCCESS)
	stopPlayback = true;
    loopFinished = true;
}

// A bunch of error-message functions
void AlarmFsck::error_to_user(const std::string& appErrMessage, const std::string& sysErrMessage){
    // sysErrMessage is supposed to come from errno or a similar error mechanism
    if(sysErrMessage.size() > 0)
	std::cerr << sysErrMessage << "\n\t" << appErrMessage << "\n";
    else
	std::cerr << "\t" << appErrMessage << "\n";
    commentField.set_text(appErrMessage);
}

void AlarmFsck::error_to_user(const AfSystemException& error){
    error_to_user(error.get_message(), error.what());
}

void AlarmFsck::error_to_user(const std::string& appErrMessage){
    error_to_user(appErrMessage, "");
}

int parseAnswerString(std::string holly)
{
    std::stringstream s;
    int result;

    s << holly;
    s >> result;

    return result;
}

void AlarmFsck::on_button_clicked()
{
    if(solvedIt) return;
    if(parseAnswerString(answerField.get_text().raw()) != randNo1*randNo2){
	commentField.set_text("Wrooong!!!");
	return;
    }
    commentField.set_text("Whoa, you did it.");
    // Cancel the playback
    int errorCode = ca_context_cancel(canberraContext, canberra_context_id);
    if (errorCode < 0)
	std::cout << ca_strerror(errorCode);
    if(hasHostages){
	try {
	    decrypt_hostage_archive();
	    decompress_hostage_archive();
	    std::string fullHostageArchivePath = prefixDir + hostage_archive;
	    free_hostages(fullHostageArchivePath);
	    // erase the uncompressed archive
	    afCommon::erase_file(fullHostageArchivePath);
	    hasHostages = false;
	}
	catch (AfSystemException& error) {error_to_user(error);}
	catch (CryptoPP::Exception& error) {error_to_user(error.what());}
    }
    solvedIt = true;
}

void AlarmFsck::free_hostages(const std::string& fullHostageArchivePath){
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

void AlarmFsck::decompress_hostage_archive(){
    // TODO: add error checking
    namespace io = boost::iostreams;
    std::ifstream fileIn(compressedPath, std::ios_base::in | std::ios_base::binary);
    io::filtering_streambuf<io::input> in;
    in.push(io::gzip_decompressor());
    in.push(fileIn);
    std::ofstream fileOut(prefixDir + hostage_archive, std::ios_base::out | std::ios_base::binary);
    io::copy(in, fileOut);
    in.pop();
    fileOut.close();
}

void AlarmFsck::encrypt_hostage_archive(){
    using namespace CryptoPP;
    rng.GenerateBlock(key, key.size());
    rng.GenerateBlock(iv, iv.size());
    CBC_Mode<AES>::Encryption enc;
    enc.SetKeyWithIV(key, key.size(), iv);
    FileSource fs(compressedPath.c_str(), true,
	    new StreamTransformationFilter(enc,
		new FileSink(encryptedPath.c_str())
		)
	    );
}

void AlarmFsck::decrypt_hostage_archive(){
    using namespace CryptoPP;
    CBC_Mode<AES>::Decryption dec;
    dec.SetKeyWithIV(key, key.size(), iv);
    FileSource fs(encryptedPath.c_str(), true,
	    new StreamTransformationFilter(dec,
		new FileSink(compressedPath.c_str())
		)
	    );
}

bool AlarmFsck::on_window_delete(GdkEventAny* event)
{
    if(!solvedIt){
	commentField.set_text("Not so fast!!!");
	return true;
    }
    return on_delete_event(event);
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

    AlarmFsck alarmFsck;

    //Shows the window and returns when it is closed.
    int ret = app->run(alarmFsck);
    std::cout << "Exit code: " << ret << std::endl;
    return ret;
}
