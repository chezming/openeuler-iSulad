# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.22

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /root/iSulad

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /root/iSulad/test

# Include any dependencies generated for this target.
include test/cutils/mainloop/CMakeFiles/mainloop_ut.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include test/cutils/mainloop/CMakeFiles/mainloop_ut.dir/compiler_depend.make

# Include the progress variables for this target.
include test/cutils/mainloop/CMakeFiles/mainloop_ut.dir/progress.make

# Include the compile flags for this target's objects.
include test/cutils/mainloop/CMakeFiles/mainloop_ut.dir/flags.make

test/cutils/mainloop/CMakeFiles/mainloop_ut.dir/__/__/mocks/mainloop_mock.cc.o: test/cutils/mainloop/CMakeFiles/mainloop_ut.dir/flags.make
test/cutils/mainloop/CMakeFiles/mainloop_ut.dir/__/__/mocks/mainloop_mock.cc.o: mocks/mainloop_mock.cc
test/cutils/mainloop/CMakeFiles/mainloop_ut.dir/__/__/mocks/mainloop_mock.cc.o: test/cutils/mainloop/CMakeFiles/mainloop_ut.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/root/iSulad/test/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object test/cutils/mainloop/CMakeFiles/mainloop_ut.dir/__/__/mocks/mainloop_mock.cc.o"
	cd /root/iSulad/test/test/cutils/mainloop && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT test/cutils/mainloop/CMakeFiles/mainloop_ut.dir/__/__/mocks/mainloop_mock.cc.o -MF CMakeFiles/mainloop_ut.dir/__/__/mocks/mainloop_mock.cc.o.d -o CMakeFiles/mainloop_ut.dir/__/__/mocks/mainloop_mock.cc.o -c /root/iSulad/test/mocks/mainloop_mock.cc

test/cutils/mainloop/CMakeFiles/mainloop_ut.dir/__/__/mocks/mainloop_mock.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/mainloop_ut.dir/__/__/mocks/mainloop_mock.cc.i"
	cd /root/iSulad/test/test/cutils/mainloop && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /root/iSulad/test/mocks/mainloop_mock.cc > CMakeFiles/mainloop_ut.dir/__/__/mocks/mainloop_mock.cc.i

test/cutils/mainloop/CMakeFiles/mainloop_ut.dir/__/__/mocks/mainloop_mock.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/mainloop_ut.dir/__/__/mocks/mainloop_mock.cc.s"
	cd /root/iSulad/test/test/cutils/mainloop && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /root/iSulad/test/mocks/mainloop_mock.cc -o CMakeFiles/mainloop_ut.dir/__/__/mocks/mainloop_mock.cc.s

test/cutils/mainloop/CMakeFiles/mainloop_ut.dir/mainloop_ut.cc.o: test/cutils/mainloop/CMakeFiles/mainloop_ut.dir/flags.make
test/cutils/mainloop/CMakeFiles/mainloop_ut.dir/mainloop_ut.cc.o: cutils/mainloop/mainloop_ut.cc
test/cutils/mainloop/CMakeFiles/mainloop_ut.dir/mainloop_ut.cc.o: test/cutils/mainloop/CMakeFiles/mainloop_ut.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/root/iSulad/test/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object test/cutils/mainloop/CMakeFiles/mainloop_ut.dir/mainloop_ut.cc.o"
	cd /root/iSulad/test/test/cutils/mainloop && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT test/cutils/mainloop/CMakeFiles/mainloop_ut.dir/mainloop_ut.cc.o -MF CMakeFiles/mainloop_ut.dir/mainloop_ut.cc.o.d -o CMakeFiles/mainloop_ut.dir/mainloop_ut.cc.o -c /root/iSulad/test/cutils/mainloop/mainloop_ut.cc

test/cutils/mainloop/CMakeFiles/mainloop_ut.dir/mainloop_ut.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/mainloop_ut.dir/mainloop_ut.cc.i"
	cd /root/iSulad/test/test/cutils/mainloop && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /root/iSulad/test/cutils/mainloop/mainloop_ut.cc > CMakeFiles/mainloop_ut.dir/mainloop_ut.cc.i

test/cutils/mainloop/CMakeFiles/mainloop_ut.dir/mainloop_ut.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/mainloop_ut.dir/mainloop_ut.cc.s"
	cd /root/iSulad/test/test/cutils/mainloop && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /root/iSulad/test/cutils/mainloop/mainloop_ut.cc -o CMakeFiles/mainloop_ut.dir/mainloop_ut.cc.s

# Object files for target mainloop_ut
mainloop_ut_OBJECTS = \
"CMakeFiles/mainloop_ut.dir/__/__/mocks/mainloop_mock.cc.o" \
"CMakeFiles/mainloop_ut.dir/mainloop_ut.cc.o"

# External object files for target mainloop_ut
mainloop_ut_EXTERNAL_OBJECTS =

test/cutils/mainloop/mainloop_ut: test/cutils/mainloop/CMakeFiles/mainloop_ut.dir/__/__/mocks/mainloop_mock.cc.o
test/cutils/mainloop/mainloop_ut: test/cutils/mainloop/CMakeFiles/mainloop_ut.dir/mainloop_ut.cc.o
test/cutils/mainloop/mainloop_ut: test/cutils/mainloop/CMakeFiles/mainloop_ut.dir/build.make
test/cutils/mainloop/mainloop_ut: /usr/local/lib64/libgtest_main.so.1.13.0
test/cutils/mainloop/mainloop_ut: /usr/local/lib64/libgmock.so
test/cutils/mainloop/mainloop_ut: /usr/local/lib64/libgmock_main.so
test/cutils/mainloop/mainloop_ut: /usr/local/lib/libisula_libutils.so
test/cutils/mainloop/mainloop_ut: test/cutils/libutils_ut.a
test/cutils/mainloop/mainloop_ut: /usr/local/lib64/libgtest.so.1.13.0
test/cutils/mainloop/mainloop_ut: /usr/local/lib/libisula_libutils.so
test/cutils/mainloop/mainloop_ut: test/cutils/mainloop/CMakeFiles/mainloop_ut.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/root/iSulad/test/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Linking CXX executable mainloop_ut"
	cd /root/iSulad/test/test/cutils/mainloop && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/mainloop_ut.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
test/cutils/mainloop/CMakeFiles/mainloop_ut.dir/build: test/cutils/mainloop/mainloop_ut
.PHONY : test/cutils/mainloop/CMakeFiles/mainloop_ut.dir/build

test/cutils/mainloop/CMakeFiles/mainloop_ut.dir/clean:
	cd /root/iSulad/test/test/cutils/mainloop && $(CMAKE_COMMAND) -P CMakeFiles/mainloop_ut.dir/cmake_clean.cmake
.PHONY : test/cutils/mainloop/CMakeFiles/mainloop_ut.dir/clean

test/cutils/mainloop/CMakeFiles/mainloop_ut.dir/depend:
	cd /root/iSulad/test && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /root/iSulad /root/iSulad/test/cutils/mainloop /root/iSulad/test /root/iSulad/test/test/cutils/mainloop /root/iSulad/test/test/cutils/mainloop/CMakeFiles/mainloop_ut.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : test/cutils/mainloop/CMakeFiles/mainloop_ut.dir/depend

