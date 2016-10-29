#ifndef GTKMM_EXAMPLEWORKER_H
#define GTKMM_EXAMPLEWORKER_H

#include <gtkmm.h>

class AlarmFuck;

class LoopPlayWorker
{
public:
  LoopPlayWorker() : m_Mutex(), finished_loop(false), stop_playback(false) {}

  // Thread function.
  void loopPlay(AlarmFuck* caller);
  void signalLoopFinish() {finished_loop = true;}
  void signalStopPlayback() {stop_playback = true;}
  bool shallStopPlayback() const { return stop_playback; }
  bool hasFinishedLoop() const { return finished_loop; }
  Glib::Threads::Mutex* getMutex() {return &m_Mutex;}


private:
  // Synchronizes access to member data.
  mutable Glib::Threads::Mutex m_Mutex;

  // Data used by both GUI thread and worker thread.
  bool finished_loop;
  bool stop_playback;
};

#endif // GTKMM_EXAMPLEWORKER_H

