# general variables
IDIR=include
BINDIR=bin
SDIR=src
CXX=g++ -g -Wno-deprecated-declarations
COMPILE_DEPS=gtkmm-3.0 libcanberra libcanberra-gtk3
CFLAGS=-I$(IDIR) `pkg-config --cflags $(COMPILE_DEPS)`
LIBS= `pkg-config --libs $(COMPILE_DEPS)`
UGLY_SUPPLEMENT_STRING= $(BINDIR)/libboost_iostreams.so.1.62.0 -ltar

# tell make where to look for types of files
vpath %.h $(IDIR)
vpath %.o $(SDIR)
vpath %.cc $(SDIR)

# top-level programs
REXEC = $(BINDIR)/alarmfuck
LEXEC = $(BINDIR)/aflaunch
HEXEC = $(BINDIR)/hibernator

# headers upon which top-level programs depend
RDEPS = alarmfuck.h loopplayworker.h common.h
LDEPS = aflaunch.h common.h affilechooser.h

# object files comprising the top-level programs
RINGEROBJ = alarmfuck.o common.o
LAUNCHEROBJ = aflaunch-ui.o aflaunch-logic.o affilechooser-ui.o affilechooser-logic.o common.o

# all the phony targets
.PHONY: move all auth clean

# default make procedure
all: $(REXEC) $(LEXEC) $(HEXEC) move

# to make parallel execution possible
move: $(REXEC) $(LEXEC) $(HEXEC)

# object compilation rule
%.o: %.cc
	$(CXX) -c -o $@ $< $(CFLAGS)

# interdependencies
common.o: common.h
alarmfuck.o: alarmfuck.h
aflaunch-ui.o aflaunch-logic.o: aflaunch.h common.h
affilechooser-ui.o: affilechooser.h aflaunch.h common.h
affilechooser-logic.o: affilechooser.h common.h

aflaunch.h: affilechooser.h
alarmfuck.h: loopplayworker.h common.h

# top-level executables' compilation rules
$(REXEC): alarmfuck.o common.o
	$(CXX) -o $@ $^ $(CFLAGS) $(LIBS) $(UGLY_SUPPLEMENT_STRING)

$(LEXEC): aflaunch-ui.o aflaunch-logic.o affilechooser-ui.o affilechooser-logic.o common.o
	$(CXX) -o $@ $^ $(CFLAGS) $(LIBS) $(UGLY_SUPPLEMENT_STRING) -lbsd

$(HEXEC): hibernator.o
	$(CXX) -o $@ $^

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
