# alarmfsck
An alarm clock for Linux that fscks with your mind. It disregards absolutely
every piece of science ever written on waking up effectively but gets the job
done. It encrypts a set of files of your choosing, hibernates the computer,
wakes it up at the requested time and presents you with a maths problem,
accompanied by horrible loud screeching sounds. Upon solving the maths problem
your files are unencrypted, while closing the program any other way means they
are lost forever.

This is a C++ project utilizing the GTK widget toolkit.

DEPENDENCIES

The following development libraries and their dependencies must be installed on
your system for the code to compile:
- gtkmm-3.0
- libcanberra-gtk3
- libcrypto++
- libtar

COMPILING AND RUNNING

From the base directory, run

	make
	sudo make auth

The pre-hibernation executable (the one you want) is bin/aflaunch. Please
ensure that your desktop environment does not present a lock screen/password
prompt when waking up from hibernation. Or, if it does but nevertheless plays
background audio, just leave it like that, as that is very much in the
masochistic spirit of this application.

ACKNOWLEDGMENTS

The preliminary audio file used for the alarm is derived from audio clips
licensed under the Creative Commons Attribution 3.0 license (available at
https://creativecommons.org/licenses/by/3.0/). The clips are as follows:

"People Screaming" by Mike Koenig, http://soundbible.com/1082-People-Screaming.html  
"Angry Chipmunk" by Mike Koenig, http://soundbible.com/1268-Angry-Chipmunk.html  
"Cat Scream" by soundbible.com contributor "Ca9", http://soundbible.com/1509-Cat-Scream.html  
"Screaming Hawk" by soundbible.com contributor "PsychoBird", http://soundbible.com/1517-Screaming-Hawk.html  

More information on the APIs used can be found at:

gtkmm - http://www.gtkmm.org  
libcanberra - http://0pointer.de/lennart/projects/libcanberra/  
crypto++ - https://www.cryptopp.com/  
libtar - http://repo.or.cz/w/libtar.git

-----------

2016, Matjaz Payrits
