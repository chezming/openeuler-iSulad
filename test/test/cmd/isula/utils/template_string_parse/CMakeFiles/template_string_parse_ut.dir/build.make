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
include test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/compiler_depend.make

# Include the progress variables for this target.
include test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/progress.make

# Include the compile flags for this target's objects.
include test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/flags.make

test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/__/__/__/__/__/src/cmd/isula/template_string_parse.c.o: test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/flags.make
test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/__/__/__/__/__/src/cmd/isula/template_string_parse.c.o: ../src/cmd/isula/template_string_parse.c
test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/__/__/__/__/__/src/cmd/isula/template_string_parse.c.o: test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/root/iSulad/test/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/__/__/__/__/__/src/cmd/isula/template_string_parse.c.o"
	cd /root/iSulad/test/test/cmd/isula/utils/template_string_parse && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/__/__/__/__/__/src/cmd/isula/template_string_parse.c.o -MF CMakeFiles/template_string_parse_ut.dir/__/__/__/__/__/src/cmd/isula/template_string_parse.c.o.d -o CMakeFiles/template_string_parse_ut.dir/__/__/__/__/__/src/cmd/isula/template_string_parse.c.o -c /root/iSulad/src/cmd/isula/template_string_parse.c

test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/__/__/__/__/__/src/cmd/isula/template_string_parse.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/template_string_parse_ut.dir/__/__/__/__/__/src/cmd/isula/template_string_parse.c.i"
	cd /root/iSulad/test/test/cmd/isula/utils/template_string_parse && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /root/iSulad/src/cmd/isula/template_string_parse.c > CMakeFiles/template_string_parse_ut.dir/__/__/__/__/__/src/cmd/isula/template_string_parse.c.i

test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/__/__/__/__/__/src/cmd/isula/template_string_parse.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/template_string_parse_ut.dir/__/__/__/__/__/src/cmd/isula/template_string_parse.c.s"
	cd /root/iSulad/test/test/cmd/isula/utils/template_string_parse && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /root/iSulad/src/cmd/isula/template_string_parse.c -o CMakeFiles/template_string_parse_ut.dir/__/__/__/__/__/src/cmd/isula/template_string_parse.c.s

test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/__/__/__/__/__/src/cmd/isula/client_show_format.c.o: test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/flags.make
test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/__/__/__/__/__/src/cmd/isula/client_show_format.c.o: ../src/cmd/isula/client_show_format.c
test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/__/__/__/__/__/src/cmd/isula/client_show_format.c.o: test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/root/iSulad/test/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building C object test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/__/__/__/__/__/src/cmd/isula/client_show_format.c.o"
	cd /root/iSulad/test/test/cmd/isula/utils/template_string_parse && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -MD -MT test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/__/__/__/__/__/src/cmd/isula/client_show_format.c.o -MF CMakeFiles/template_string_parse_ut.dir/__/__/__/__/__/src/cmd/isula/client_show_format.c.o.d -o CMakeFiles/template_string_parse_ut.dir/__/__/__/__/__/src/cmd/isula/client_show_format.c.o -c /root/iSulad/src/cmd/isula/client_show_format.c

test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/__/__/__/__/__/src/cmd/isula/client_show_format.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/template_string_parse_ut.dir/__/__/__/__/__/src/cmd/isula/client_show_format.c.i"
	cd /root/iSulad/test/test/cmd/isula/utils/template_string_parse && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /root/iSulad/src/cmd/isula/client_show_format.c > CMakeFiles/template_string_parse_ut.dir/__/__/__/__/__/src/cmd/isula/client_show_format.c.i

test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/__/__/__/__/__/src/cmd/isula/client_show_format.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/template_string_parse_ut.dir/__/__/__/__/__/src/cmd/isula/client_show_format.c.s"
	cd /root/iSulad/test/test/cmd/isula/utils/template_string_parse && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /root/iSulad/src/cmd/isula/client_show_format.c -o CMakeFiles/template_string_parse_ut.dir/__/__/__/__/__/src/cmd/isula/client_show_format.c.s

test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/template_string_parse_ut.cc.o: test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/flags.make
test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/template_string_parse_ut.cc.o: cmd/isula/utils/template_string_parse/template_string_parse_ut.cc
test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/template_string_parse_ut.cc.o: test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/root/iSulad/test/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building CXX object test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/template_string_parse_ut.cc.o"
	cd /root/iSulad/test/test/cmd/isula/utils/template_string_parse && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/template_string_parse_ut.cc.o -MF CMakeFiles/template_string_parse_ut.dir/template_string_parse_ut.cc.o.d -o CMakeFiles/template_string_parse_ut.dir/template_string_parse_ut.cc.o -c /root/iSulad/test/cmd/isula/utils/template_string_parse/template_string_parse_ut.cc

test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/template_string_parse_ut.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/template_string_parse_ut.dir/template_string_parse_ut.cc.i"
	cd /root/iSulad/test/test/cmd/isula/utils/template_string_parse && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /root/iSulad/test/cmd/isula/utils/template_string_parse/template_string_parse_ut.cc > CMakeFiles/template_string_parse_ut.dir/template_string_parse_ut.cc.i

test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/template_string_parse_ut.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/template_string_parse_ut.dir/template_string_parse_ut.cc.s"
	cd /root/iSulad/test/test/cmd/isula/utils/template_string_parse && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /root/iSulad/test/cmd/isula/utils/template_string_parse/template_string_parse_ut.cc -o CMakeFiles/template_string_parse_ut.dir/template_string_parse_ut.cc.s

# Object files for target template_string_parse_ut
template_string_parse_ut_OBJECTS = \
"CMakeFiles/template_string_parse_ut.dir/__/__/__/__/__/src/cmd/isula/template_string_parse.c.o" \
"CMakeFiles/template_string_parse_ut.dir/__/__/__/__/__/src/cmd/isula/client_show_format.c.o" \
"CMakeFiles/template_string_parse_ut.dir/template_string_parse_ut.cc.o"

# External object files for target template_string_parse_ut
template_string_parse_ut_EXTERNAL_OBJECTS =

test/cmd/isula/utils/template_string_parse/template_string_parse_ut: test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/__/__/__/__/__/src/cmd/isula/template_string_parse.c.o
test/cmd/isula/utils/template_string_parse/template_string_parse_ut: test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/__/__/__/__/__/src/cmd/isula/client_show_format.c.o
test/cmd/isula/utils/template_string_parse/template_string_parse_ut: test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/template_string_parse_ut.cc.o
test/cmd/isula/utils/template_string_parse/template_string_parse_ut: test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/build.make
test/cmd/isula/utils/template_string_parse/template_string_parse_ut: /usr/local/lib64/libgtest_main.so.1.13.0
test/cmd/isula/utils/template_string_parse/template_string_parse_ut: /usr/local/lib/libisula_libutils.so
test/cmd/isula/utils/template_string_parse/template_string_parse_ut: test/cutils/libutils_ut.a
test/cmd/isula/utils/template_string_parse/template_string_parse_ut: /usr/local/lib64/libgtest.so.1.13.0
test/cmd/isula/utils/template_string_parse/template_string_parse_ut: /usr/local/lib/libisula_libutils.so
test/cmd/isula/utils/template_string_parse/template_string_parse_ut: test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/root/iSulad/test/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Linking CXX executable template_string_parse_ut"
	cd /root/iSulad/test/test/cmd/isula/utils/template_string_parse && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/template_string_parse_ut.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/build: test/cmd/isula/utils/template_string_parse/template_string_parse_ut
.PHONY : test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/build

test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/clean:
	cd /root/iSulad/test/test/cmd/isula/utils/template_string_parse && $(CMAKE_COMMAND) -P CMakeFiles/template_string_parse_ut.dir/cmake_clean.cmake
.PHONY : test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/clean

test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/depend:
	cd /root/iSulad/test && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /root/iSulad /root/iSulad/test/cmd/isula/utils/template_string_parse /root/iSulad/test /root/iSulad/test/test/cmd/isula/utils/template_string_parse /root/iSulad/test/test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : test/cmd/isula/utils/template_string_parse/CMakeFiles/template_string_parse_ut.dir/depend

