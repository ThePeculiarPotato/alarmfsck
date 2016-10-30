#ifndef ORG_WALRUS_ALARMFUCK_ALARMFUCK_H
#define ORG_WALRUS_ALARMFUCK_ALARMFUCK_H

#include <gtkmm/button.h>
#include <gtkmm/window.h>
#include <glibmm/dispatcher.h>
#include <canberra-gtk.h>
#include "loopplayworker.h"
#include "aflaunch.h"

#define PADDING 10
#define DATA_DIR "data/"
#define BIN_DIR "bin/"
#define HOSTAGE_FILE "hostages.af"
#define HOSTAGE_ARCHIVE "hostages.tar"
#define HOSTAGE_COMPRESSED "hostages.tar.gz"
#define HIB_EXEC "hibernator"
#define AUDIO_FILE "rullGenocide.oga"
#define FILE_DELIM '/'
#define SUGGESTED_HOURS 8

class AlarmFuck : public Gtk::Window
{

public:
	AlarmFuck();
	virtual ~AlarmFuck();
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
	ca_context* context;
	ca_proplist* props;

	// file-tree
	std::string baseDir;
	bool hasHostages;
	bool decompress_hostage_archive();

	//Multithreading
	Glib::Dispatcher dispatcher;
	LoopPlayWorker worker;
	Glib::Threads::Thread* workerThread;
private:
	gint32 randNo1, randNo2;
};

#endif // ORG_WALRUS_ALARMFUCK_ALARMFUCK_H
