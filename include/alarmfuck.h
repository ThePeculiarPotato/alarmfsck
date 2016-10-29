#ifndef ORG_WALRUS_ALARMFUCK_ALARMFUCK_H
#define ORG_WALRUS_ALARMFUCK_ALARMFUCK_H

#include <gtkmm/button.h>
#include <gtkmm/window.h>
#include <canberra-gtk.h>
#include "loopplayworker.h"

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

	//Multithreading
	Glib::Dispatcher dispatcher;
	LoopPlayWorker worker;
	Glib::Threads::Thread* workerThread;
private:
	gint32 randNo1, randNo2;
};

#endif // ORG_WALRUS_ALARMFUCK_ALARMFUCK_H
