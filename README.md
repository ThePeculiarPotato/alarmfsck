# alarmfsck
An alarm clock for Linux that fscks with your mind. It disregards absolutely
every piece of science ever written on waking up effectively but gets the job
done. It encrypts a set of files of your choosing, hibernates the computer,
wakes it up at the requested time and presents you with a maths problem,
accompanied by horrible loud screeching sounds. Upon solving the maths problem
your files are unencrypted while closing the program any other way means they
are lost forever.

DEPENDENCIES

The following development libraries must be installed on your system for the
code to compile:
- gtkmm-3.0
- libcanberra
- libcanberra-gtk3
- libtar
- libbsd
- libboost-iostreams

The last of these has not been playing nice with pkg-config and the explicit
location of the libboost_iostreams.so shared object and header files had to be
used in the makefile. These locations might not be the same on your system,
please check and modify accordingly. I might bundle the relevant files with the
project later.

COMPILING AND RUNNING

Edit your Makefile if necessary as per above and then run, from the base
directory,

	make
	sudo make auth

The pre-hibernation executable (the one you want) is bin/aflaunch.

ACKNOWLEDGMENTS

The preliminary audio file used for the alarm is derived from audio clips
licensed under the Creative Commons Attribution 3.0 license (available at
https://creativecommons.org/licenses/by/3.0/). The clips are as follows:

"People Screaming" by Mike Koenig, http://soundbible.com/1082-People-Screaming.html  
"Angry Chipmunk" by Mike Koenig, http://soundbible.com/1268-Angry-Chipmunk.html  
"Cat Scream" by soundbible.com contributor "Ca9", http://soundbible.com/1509-Cat-Scream.html  
"Screaming Hawk" by soundbible.com contributor "PsychoBird", http://soundbible.com/1517-Screaming-Hawk.html  