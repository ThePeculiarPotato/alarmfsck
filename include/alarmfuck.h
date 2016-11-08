#ifndef ORG_WALRUS_ALARMFUCK_ALARMFUCK_H
#define ORG_WALRUS_ALARMFUCK_ALARMFUCK_H

#include "loopplayworker.h"
#include "common.h"
#include <gtkmm/button.h>
#include <gtkmm/window.h>
#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/entry.h>
#include <glibmm/dispatcher.h>
#include <canberra-gtk.h>

class AlarmFuck : public Gtk::Window
{

public:
    AlarmFuck();
    void play_sound(LoopPlayWorker*);

protected:
    //Signal handlers:
    void on_button_clicked();
    bool on_window_delete(GdkEventAny*);

    //Member widgets:
    Gtk::Box vBox;
    Gtk::Button m_button;
    Gtk::Label questionField, commentField;
    Gtk::Entry answerField;

    // canberra-related
    ca_context* context;
    ca_proplist* props;

    // file-tree
    std::string baseDir;
    bool hasHostages;
    bool decompress_hostage_archive();
    void free_hostages(const std::string&);
    void error_to_user(const std::string&, const std::string&);
    void error_to_user(const AfSystemException& error);
    void error_to_user(const std::string&);
    bool erase_file(std::string);

    //Multithreading
    Glib::Dispatcher dispatcher;
    LoopPlayWorker worker;
    Glib::Threads::Thread* workerThread;
private:
    gint32 randNo1, randNo2;
};

#endif // ORG_WALRUS_ALARMFUCK_ALARMFUCK_H
