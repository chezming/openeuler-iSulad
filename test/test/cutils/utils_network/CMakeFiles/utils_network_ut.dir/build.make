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
include test/cutils/utils_network/CMakeFiles/utils_network_ut.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include test/cutils/utils_network/CMakeFiles/utils_network_ut.dir/compiler_depend.make

# Include the progress variables for this target.
include test/cutils/utils_network/CMakeFiles/utils_network_ut.dir/progress.make

# Include the compile flags for this target's objects.
include test/cutils/utils_network/CMakeFiles/utils_network_ut.dir/flags.make

test/cutils/utils_network/CMakeFiles/utils_network_ut.dir/utils_network_ut.cc.o: test/cutils/utils_network/CMakeFiles/utils_network_ut.dir/flags.make
test/cutils/utils_network/CMakeFiles/utils_network_ut.dir/utils_network_ut.cc.o: cutils/utils_network/utils_network_ut.cc
test/cutils/utils_network/CMakeFiles/utils_network_ut.dir/utils_network_ut.cc.o: test/cutils/utils_network/CMakeFiles/utils_network_ut.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/root/iSulad/test/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object test/cutils/utils_network/CMakeFiles/utils_network_ut.dir/utils_network_ut.cc.o"
	cd /root/iSulad/test/test/cutils/utils_network && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT test/cutils/utils_network/CMakeFiles/utils_network_ut.dir/utils_network_ut.cc.o -MF CMakeFiles/utils_network_ut.dir/utils_network_ut.cc.o.d -o CMakeFiles/utils_network_ut.dir/utils_network_ut.cc.o -c /root/iSulad/test/cutils/utils_network/utils_network_ut.cc

test/cutils/utils_network/CMakeFiles/utils_network_ut.dir/utils_network_ut.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/utils_network_ut.dir/utils_network_ut.cc.i"
	cd /root/iSulad/test/test/cutils/utils_network && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /root/iSulad/test/cutils/utils_network/utils_network_ut.cc > CMakeFiles/utils_network_ut.dir/utils_network_ut.cc.i

test/cutils/utils_network/CMakeFiles/utils_network_ut.dir/utils_network_ut.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/utils_network_ut.dir/utils_network_ut.cc.s"
	cd /root/iSulad/test/test/cutils/utils_network && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /root/iSulad/test/cutils/utils_network/utils_network_ut.cc -o CMakeFiles/utils_network_ut.dir/utils_network_ut.cc.s

# Object files for target utils_network_ut
utils_network_ut_OBJECTS = \
"CMakeFiles/utils_network_ut.dir/utils_network_ut.cc.o"

# External object files for target utils_network_ut
utils_network_ut_EXTERNAL_OBJECTS =

test/cutils/utils_network/utils_network_ut: test/cutils/utils_network/CMakeFiles/utils_network_ut.dir/utils_network_ut.cc.o
test/cutils/utils_network/utils_network_ut: test/cutils/utils_network/CMakeFiles/utils_network_ut.dir/build.make
test/cutils/utils_network/utils_network_ut: /usr/local/lib64/libgtest_main.so.1.13.0
test/cutils/utils_network/utils_network_ut: /usr/local/lib64/libgmock.so
test/cutils/utils_network/utils_network_ut: /usr/local/lib64/libgmock_main.so
test/cutils/utils_network/utils_network_ut: /usr/local/lib/libisula_libutils.so
test/cutils/utils_network/utils_network_ut: test/cutils/libutils_ut.a
test/cutils/utils_network/utils_network_ut: /usr/local/lib64/libgtest.so.1.13.0
test/cutils/utils_network/utils_network_ut: /usr/local/lib/libisula_libutils.so
test/cutils/utils_network/utils_network_ut: test/cutils/utils_network/CMakeFiles/utils_network_ut.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/root/iSulad/test/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable utils_network_ut"
	cd /root/iSulad/test/test/cutils/utils_network && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/utils_network_ut.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
test/cutils/utils_network/CMakeFiles/utils_network_ut.dir/build: test/cutils/utils_network/utils_network_ut
.PHONY : test/cutils/utils_network/CMakeFiles/utils_network_ut.dir/build

test/cutils/utils_network/CMakeFiles/utils_network_ut.dir/clean:
	cd /root/iSulad/test/test/cutils/utils_network && $(CMAKE_COMMAND) -P CMakeFiles/utils_network_ut.dir/cmake_clean.cmake
.PHONY : test/cutils/utils_network/CMakeFiles/utils_network_ut.dir/clean

test/cutils/utils_network/CMakeFiles/utils_network_ut.dir/depend:
	cd /root/iSulad/test && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /root/iSulad /root/iSulad/test/cutils/utils_network /root/iSulad/test /root/iSulad/test/test/cutils/utils_network /root/iSulad/test/test/cutils/utils_network/CMakeFiles/utils_network_ut.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : test/cutils/utils_network/CMakeFiles/utils_network_ut.dir/depend

