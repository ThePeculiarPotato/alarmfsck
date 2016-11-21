#include "alarmfsck.h"
#include "common.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <functional>
#include <cmath>
#include <random>
#include <gtkmm/application.h>
extern "C" {
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <libtar.h>
#include <libgen.h>
#include <sys/types.h>
}
#include <cryptopp/filters.h>
#include <cryptopp/modes.h>
#include <cryptopp/files.h>
#include <cryptopp/gzip.h>

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
    /* TODO: Allow the maximum product of the two numbers and the lower bounds
     * for individual factors to be set by the user. */
    const double lower_product_limit = std::log(100), upper_product_limit = std::log(1000), lower_factor_limit = std::log(3);
    std::default_random_engine randEng(std::chrono::system_clock::now().time_since_epoch().count());
    double productLog = std::uniform_real_distribution<double>(lower_product_limit, upper_product_limit)(randEng);
    double factorLog1 = std::uniform_real_distribution<double>(lower_factor_limit, productLog - lower_factor_limit)(randEng);
    randFactor1 = (int) (std::exp(factorLog1) + .5);
    randFactor2 = (int) (std::exp(productLog - factorLog1) + .5);
    std::stringstream ss;
    ss << randFactor1 << " * " << randFactor2;

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
    dataDir = baseDir + data_dir;
    std::string audioPath = dataDir + audio_file;
    
    // read files from $HOME/.alarmfsck/
    std::string homeDir = getenv("HOME");
    userDir = homeDir + "/." + package_name + "/";
    if(access(userDir.c_str(), W_OK | X_OK) == -1 && mkdir(userDir.c_str(), 0777) == -1){
	error_to_user("Could not access " + userDir + ": Using current path.");
	userDir = "";
    }

    archivePath = userDir + hostage_archive;
    compressedPath = userDir + hostage_compressed;
    encryptedPath = userDir + hostage_encrypted;

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
	std::cerr << appErrMessage << "\n\t" << sysErrMessage << "\n";
    else
	std::cerr << appErrMessage << "\n";
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
    if(parseAnswerString(answerField.get_text().raw()) != randFactor1 * randFactor2){
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
	    decrypt_decompress_hostage_archive();
	    free_hostages(archivePath);
	    // erase the leftovers
	    afCommon::erase_file(encryptedPath);
	    afCommon::erase_file(archivePath);
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

void AlarmFsck::decrypt_decompress_hostage_archive(){
    using namespace CryptoPP;
    CBC_Mode<AES>::Decryption dec;
    dec.SetKeyWithIV(key, key.size(), iv);
    FileSource fs(encryptedPath.c_str(), true,
	    new StreamTransformationFilter(dec,
		new Gunzip(
		    new FileSink(archivePath.c_str())
		    )
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
