#ifndef ORG_WALRUS_ALARMFSCK_ALARMFSCK_H
#define ORG_WALRUS_ALARMFSCK_ALARMFSCK_H

#include "common.h"
#include <gtkmm/button.h>
#include <gtkmm/window.h>
#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/entry.h>
#include <glibmm/dispatcher.h>
#include <canberra-gtk.h>
#include <cryptopp/osrng.h>

class AlarmFsck : public Gtk::Window
{

public:
    AlarmFsck();

private:
    //Signal handlers:
    void on_button_clicked();
    bool on_window_delete(GdkEventAny*);

    //Member widgets:
    Gtk::Box vBox;
    Gtk::Button m_button;
    Gtk::Label questionField, commentField;
    Gtk::Entry answerField;

    // canberra-related
    ca_context* canberraContext;
    ca_proplist* canberraProps;

    // thread flags - see comment in alarmfsck.cc on why they're not atomic
    static bool loopFinished;
    static bool stopPlayback;
    // thread tasks
    static void playback_looper(ca_context* canCon, ca_proplist* canProp);
    static void canberra_callback(ca_context *, uint32_t, int, void *);

    // encryption
    CryptoPP::SecByteBlock key, iv;
    CryptoPP::AutoSeededRandomPool rng;

    bool hasHostages;
    bool solvedIt;

    // file-tree
    std::string prefixDir;
    std::string compressedPath;
    std::string encryptedPath;
    std::string archivePath;
    void encrypt_hostage_archive();
    void decrypt_decompress_hostage_archive();
    void free_hostages(const std::string&);
    void error_to_user(const std::string&, const std::string&);
    void error_to_user(const AfSystemException& error);
    void error_to_user(const std::string&);
    bool erase_file(std::string);

    // rands
    int randFactor1, randFactor2;
};

#endif // ORG_WALRUS_ALARMFSCK_ALARMFSCK_H
