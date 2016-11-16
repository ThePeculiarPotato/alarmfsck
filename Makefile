# general variables
IDIR=include
BINDIR=bin
SDIR=src
CXX=g++ -g -Wno-deprecated-declarations
COMPILE_DEPS=gtkmm-3.0 libcanberra-gtk3 libcrypto++
CFLAGS=-I$(IDIR) `pkg-config --cflags $(COMPILE_DEPS)`
LIBS= `pkg-config --libs $(COMPILE_DEPS)`

# tell make where to look for types of files
vpath %.h $(IDIR)
vpath %.o $(SDIR)
vpath %.cc $(SDIR)

# top-level programs
REXEC = $(BINDIR)/alarmfsck
LEXEC = $(BINDIR)/aflaunch
HEXEC = $(BINDIR)/hibernator

# object files comprising the top-level programs
RINGEROBJ = alarmfsck.o common.o
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
alarmfsck.o: alarmfsck.h
aflaunch-ui.o aflaunch-logic.o: aflaunch.h common.h
affilechooser-ui.o: affilechooser.h aflaunch.h common.h
affilechooser-logic.o: affilechooser.h common.h
hibernator.o: common.h

aflaunch.h: affilechooser.h
alarmfsck.h: common.h

# top-level executables' compilation rules
$(REXEC): alarmfsck.o common.o
	$(CXX) -o $@ $^ $(CFLAGS) $(LIBS) -ltar

$(LEXEC): aflaunch-ui.o aflaunch-logic.o affilechooser-ui.o affilechooser-logic.o common.o
	$(CXX) -o $@ $^ $(CFLAGS) $(LIBS) -ltar

$(HEXEC): hibernator.o common.o
	$(CXX) -o $@ $^ $(LIBS)

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
