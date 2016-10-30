# general variables
IDIR=include
BINDIR=bin
SDIR=src
CC=g++ -g -Wno-deprecated-declarations
COMPILE_DEPS=gtkmm-3.0 libcanberra libcanberra-gtk3
CFLAGS=-I$(IDIR) `pkg-config --cflags $(COMPILE_DEPS)`
LIBS= `pkg-config --libs $(COMPILE_DEPS)`
UGLY_SUPPLEMENT_STRING= $(BINDIR)/libboost_iostreams.so.1.61.0 -ltar

# tell make where to look for types of files
vpath %.h $(IDIR)
vpath %.o $(SDIR)
vpath %.cc $(SDIR)

# top-level programs
REXEC = $(BINDIR)/alarmfuck
LEXEC = $(BINDIR)/aflaunch
HEXEC = $(BINDIR)/hibernator

# headers upon which top-level programs depend
RDEPS = alarmfuck.h loopplayworker.h
LDEPS = aflaunch.h

# object files comprising the top-level programs
RINGEROBJ = alarmfuck.o
LAUNCHEROBJ = aflaunch.o affilechooser.o pathhashlist.o

# all the phony targets
.PHONY: move all auth clean

# default make procedure
all: $(REXEC) $(LEXEC) $(HEXEC) move

# to make parallel execution possible
move: $(REXEC) $(LEXEC) $(HEXEC)

# object compilation rule
%.o: %.cc
	$(CC) -c -o $@ $< $(CFLAGS)

# recompile objects if relevant headers change
$(RINGEROBJ): $(RDEPS)
$(LAUNCHEROBJ): $(LDEPS)

# top-level executables' compilation rules
$(REXEC): $(RINGEROBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS) $(UGLY_SUPPLEMENT_STRING)

$(LEXEC): $(LAUNCHEROBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS) $(UGLY_SUPPLEMENT_STRING) -lbsd

$(HEXEC): hibernator.o
	$(CC) -o $@ $^

# unclutter the base directory and move object files to src/
move:
	@if [ "`echo *.o`" != '*.o' ]; then mv *.o src/; fi

# allow hibernator to hibernate system, requires root privilege
auth:
	chown root $(HEXEC)
	chmod u+s $(HEXEC)

# remove object files
clean:
	rm -f *.o $(SDIR)/*.o data/hostages.tar*
