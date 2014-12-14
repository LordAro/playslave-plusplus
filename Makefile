# This file is part of playd.
# playd is licenced under the MIT license: see LICENSE.txt.

#
# This is the template for GNU Makefile for playd.
# Use `config.sh` to generate the full Makefile.
#
# Anything of the form %%FOO%% will be filled in by config.sh.
#

## BEGIN USER-CHANGEABLE VARIABLES ##

# Where to put the object files and other intermediate fluff.
builddir ?= /home/mattbw/ury-playd/build

# Where the source resides.
srcdir ?= /home/mattbw/ury-playd/src

# The warning flags to use when building playd.
WARNS ?= -Wall -Wextra -Werror

# Programs used during building.
CC         ?= clang
CXX        ?= clang++
GZIP       ?= gzip -9 --stdout
INSTALL    ?= install
PKG_CONFIG ?= pkg-config
FORMAT     ?= clang-format -i
DOXYGEN    ?= doxygen
GIT        ?= git
GROFF_HTML ?= mandoc -Thtml -Ostyle=man.css
# If `mandoc` is not available, `groff -Thtml -mdoc` may work.

# The C standard used to compile playd.
# This should usually be 'c99'.
C_STD ?= c99

# The C++ standard used to compile playd.
# This should usually be 'c++11', but older compilers might need
# 'c++0x'.
CXX_STD ?= c++11

# Variables used to decide where to install playd and its man pages.
prefix      ?= /usr/local
bindir      ?= $(prefix)/bin
mandir      ?= /usr/share/man/man1

## END USER-CHANGEABLE VARIABLES ##

# The name of the program.  This is hardcoded in several places, such as the
# man page, so users are not recommended to change it.
NAME = playd

# Calculate the path to the outputted program.
BIN = $(builddir)/$(NAME)

# Where the raw man page is, and where its processed forms should go.
MAN_SRC   = $(srcdir)/$(NAME).1
MAN_GZ    = $(builddir)/$(NAME).1.gz
MAN_HTML  = $(builddir)/$(NAME).1.html

# This should include all of the source directories for playd,
# excluding any special ones defined below.  The root source directory is
# implied.
OWN_SUBDIRS = audio audio/sources io player
SUBDIRS     = $(OWN_SUBDIRS) contrib/pa_ringbuffer

# Now we work out which libraries to use, using pkg-config.
# These packages are always used: the PortAudio C library, libsox, and libuv.
PKGS = sox libuv flac++ libmpg123 vorbisfile portaudio-2.0 

# PortAudio's C++ bindings aren't always available, so we bundle them.
# However, if there is a PortAudioCPP package available, we can make use of it
# instead.
NO_SYS_PORTAUDIOCXX ?= 1
ifneq "$(NO_SYS_PORTAUDIOCXX)" ""
  # Add the bundled bindings to the list of things to compile.
  PORTAUDIOCPP_DIR  = contrib/portaudiocpp
  CXXFLAGS         += -I"$(srcdir)/$(PORTAUDIOCPP_DIR)"
  SUBDIRS          += $(PORTAUDIOCPP_DIR)
else
  PKGS += portaudiocpp
endif

# Set up the flags needed to use the packages.
PKG_CFLAGS  += `$(PKG_CONFIG) --cflags $(PKGS)`
PKG_LDFLAGS += `$(PKG_CONFIG) --libs $(PKGS)`

# Now we make up the source and object directory sets...
SRC_SUBDIRS = $(srcdir) $(addprefix $(srcdir)/,$(SUBDIRS))
OBJ_SUBDIRS = $(builddir) $(builddir)/tests $(addprefix $(builddir)/,$(SUBDIRS))

# ...And find the sources to compile and the objects they make.
SOURCES  = /home/mattbw/ury-playd/src/cmd_result.cpp /home/mattbw/ury-playd/src/cmd.cpp /home/mattbw/ury-playd/src/player/player_file.cpp /home/mattbw/ury-playd/src/player/player_state.cpp /home/mattbw/ury-playd/src/player/player_position.cpp /home/mattbw/ury-playd/src/player/player.cpp /home/mattbw/ury-playd/src/audio/audio.cpp /home/mattbw/ury-playd/src/audio/sources/sox.cpp /home/mattbw/ury-playd/src/audio/sources/mp3.cpp /home/mattbw/ury-playd/src/audio/audio_source.cpp /home/mattbw/ury-playd/src/audio/ringbuffer.cpp /home/mattbw/ury-playd/src/audio/audio_system.cpp /home/mattbw/ury-playd/src/audio/audio_sink.cpp /home/mattbw/ury-playd/src/io/io_core.cpp /home/mattbw/ury-playd/src/io/io_response.cpp /home/mattbw/ury-playd/src/io/tokeniser.cpp /home/mattbw/ury-playd/src/main.cpp /home/mattbw/ury-playd/src/errors.cpp 
OBJECTS  = /home/mattbw/ury-playd/build/cmd_result.o /home/mattbw/ury-playd/build/cmd.o /home/mattbw/ury-playd/build/player/player_file.o /home/mattbw/ury-playd/build/player/player_state.o /home/mattbw/ury-playd/build/player/player_position.o /home/mattbw/ury-playd/build/player/player.o /home/mattbw/ury-playd/build/audio/audio.o /home/mattbw/ury-playd/build/audio/sources/sox.o /home/mattbw/ury-playd/build/audio/sources/mp3.o /home/mattbw/ury-playd/build/audio/audio_source.o /home/mattbw/ury-playd/build/audio/ringbuffer.o /home/mattbw/ury-playd/build/audio/audio_system.o /home/mattbw/ury-playd/build/audio/audio_sink.o /home/mattbw/ury-playd/build/io/io_core.o /home/mattbw/ury-playd/build/io/io_response.o /home/mattbw/ury-playd/build/io/tokeniser.o /home/mattbw/ury-playd/build/main.o /home/mattbw/ury-playd/build/errors.o 
CSOURCES = /home/mattbw/ury-playd/src/contrib/pa_ringbuffer/pa_ringbuffer.c 
COBJECTS = /home/mattbw/ury-playd/build/contrib/pa_ringbuffer/pa_ringbuffer.o 

# When running unit tests, we add in the test objects, defined below.
# Note that main.o is NOT built during testing, because it contains the main
# entry point.
TEST_SOURCES  = $(wildcard $(srcdir)/tests/*.cpp)
TEST_OBJECTS  = $(patsubst $(srcdir)%,$(builddir)%,$(TEST_SOURCES:.cpp=.o))
TEST_OBJECTS += $(filter-out $(builddir)/main.o,$(OBJECTS))
TEST_BIN      = $(builddir)/$(NAME)_test

# These are used for source transformations, such as formatting.
# We don't want to disturb contributed source with these.
OWN_SRC_SUBDIRS = $(srcdir) $(addprefix $(srcdir)/,$(OWN_SUBDIRS))
OWN_SOURCES     = $(foreach dir,$(OWN_SRC_SUBDIRS),$(wildcard $(dir)/*.cpp))
OWN_CSOURCES    = $(foreach dir,$(OWN_SRC_SUBDIRS),$(wildcard $(dir)/*.c))
OWN_HEADERS     = $(foreach dir,$(OWN_SRC_SUBDIRS),$(wildcard $(dir)/*.hpp))
OWN_CHEADERS    = $(foreach dir,$(OWN_SRC_SUBDIRS),$(wildcard $(dir)/*.h))
TO_FORMAT       = $(OWN_SOURCES) $(OWN_CSOURCES) $(OWN_HEADERS) $(OWN_CHEADERS)

# Version stuff
CXXFLAGS += -D PD_VERSION=\"v0.2.0-6-ga9aedb1\"

# Now set up the flags needed for playd.
# The -I/usr/include, incidentally, is to stop certain misbehaving libraries from
# overriding the C standard library with their own badly named files.
CFLAGS   += -c $(WARNS) $(PKG_CFLAGS)  -g -std=$(C_STD)
CXXFLAGS += -c $(WARNS) $(PKG_CFLAGS)  -I/usr/include -g -std=$(CXX_STD)
LDFLAGS  += $(PKG_LDFLAGS)

## BEGIN RULES ##

.PHONY: clean mkdir install format gh-pages doc coverage

all: mkdir $(BIN) man

#
# Program
#

# Rule for making playd itself.
$(BIN): $(COBJECTS) $(OBJECTS)
	@echo LINK $@
	@$(CXX) $(COBJECTS) $(OBJECTS) $(LDFLAGS) -o $@

# Rule for compiling C code.
$(builddir)/%.o: $(srcdir)/%.c
	@echo CC $@
	@$(CC) $(CFLAGS) $< -o $@

# Rules for compiling C++ code.
$(builddir)/%.o: $(srcdir)/%.cxx
	@echo CXX $@
	@$(CXX) $(CXXFLAGS) $< -o $@
$(builddir)/%.o: $(srcdir)/%.cpp
	@echo CXX $@
	@$(CXX) $(CXXFLAGS) $< -o $@

#
# Man pages
#

# Rule for making the man pages.
man: $(MAN_GZ)
# Rule for making compressed versions of man pages.
$(builddir)/%.1.gz: $(srcdir)/%.1
	@echo GZIP $@
	@< $< $(GZIP) > $@

# Rule for making HTML versions of man pages.
$(builddir)/%.1.html: $(srcdir)/%.1
	@echo 'GROFF(html)' $@
	@< $< $(GROFF_HTML) > $@

#
# Documentation
#

# Updates the GitHub Pages documentation.
gh-pages: doc $(MAN_HTML)
	env MAN_HTML=$(MAN_HTML) ./make_gh_pages.sh

# Builds the documentation, using doxygen.
doc:
	$(DOXYGEN)

#
# Tests
#

test: mkdir $(TEST_BIN)
	@echo TEST
	@$(TEST_BIN)

$(TEST_BIN): $(COBJECTS) $(TEST_OBJECTS)
	@echo LINK $@
	@$(CXX) $(COBJECTS) $(TEST_OBJECTS) $(LDFLAGS) -o $@

#
# Special targets
#

# Make sure we clean up coverage artefacts in a make clean, too.
COV_ARTEFACTS  = $(OBJECTS:.o=.gcno)
COV_ARTEFACTS += $(OBJECTS:.o=.gcda)
COV_ARTEFACTS += $(COBJECTS:.o=.gcda)
COV_ARTEFACTS += $(COBJECTS:.o=.gcno)

# Cleans up the results of a previous build.
clean:
	@echo CLEAN
	@rm -f $(OBJECTS) $(COBJECTS) $(MAN_HTML) $(MAN_GZ) $(BIN)
	@rm -f $(TEST_OBJECTS) $(TEST_BIN)
	@rm -f $(COV_ARTEFACTS)

# Makes the build subdirectories.
mkdir:
	mkdir -p $(OBJ_SUBDIRS)

# Installs the compiled binary to `bindir`, and the man page to `mandir`.
install: $(BIN) $(MAN_GZ)
	$(INSTALL) $(BIN) $(bindir)
	-$(INSTALL) $(MAN_GZ) $(mandir)

# Runs a formatter over the sources.
format: $(TO_FORMAT)
	@echo FORMAT $^
	@$(FORMAT) $^

coverage: CXXFLAGS += -fprofile-arcs -ftest-coverage
coverage: LDFLAGS += -fprofile-arcs -ftest-coverage
coverage: clean mkdir test
