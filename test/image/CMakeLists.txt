project(iSulad_UT)

add_subdirectory(oci)

SET(EXE image_ut)

# Create your target (executable or library)
add_executable(${EXE})

# Add your main source file
target_sources(${EXE} PRIVATE image_ut.cc)

# Automatically add and include related .c files
# file(GLOB_RECURSE SOURCES_AND_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/../../src/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/../../src/*.h")

# Exclude the main.c file
# list(FILTER SOURCES_AND_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/../../src/cmd/*")

file(GLOB_RECURSE SOURCES_AND_HEADERS 
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src/utils/*.c" 
	"${CMAKE_CURRENT_SOURCE_DIR}/../../src/utils/*.h" 
	"${CMAKE_CURRENT_SOURCE_DIR}/../../src/daemon/common/*.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../src/daemon/config/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src/daemon/modules/*.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../src/daemon/executor/image_cb/*.c" 
	"${CMAKE_CURRENT_SOURCE_DIR}/../../src/daemon/*.h" 
	"${CMAKE_CURRENT_SOURCE_DIR}/../../src/common/*"
	"${CMAKE_CURRENT_SOURCE_DIR}/../mocks/image_mock.cc")

list(FILTER SOURCES_AND_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/../../src/daemon/modules/image/embedded/*")
list(FILTER SOURCES_AND_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/../../src/daemon/modules/image/oci/storage/remote_layer_support/*")
list(FILTER SOURCES_AND_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/../../src/daemon/modules/runtime/shim/*")
list(FILTER SOURCES_AND_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/../../src/daemon/modules/image/image.c")
list(FILTER SOURCES_AND_HEADERS EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/../../src/daemon/modules/service/service_image.c")

# message("files: ${SOURCES_AND_HEADERS}")

# Add all the collected .c files to the target
target_sources(${EXE} PRIVATE ${SOURCES_AND_HEADERS})

# Include directories containing headers
foreach(SOURCE_FILE ${SOURCES_AND_HEADERS})
    get_filename_component(SOURCE_DIR ${SOURCE_FILE} DIRECTORY)
    target_include_directories(${EXE} PRIVATE ${SOURCE_DIR})
	# message("directory: ${SOURCE_DIR}")
endforeach()
target_include_directories(${EXE} PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/utils
	${CMAKE_CURRENT_SOURCE_DIR}/../mocks
	)

target_link_libraries(${EXE} ${GTEST_BOTH_LIBRARIES} ${GMOCK_LIBRARY} ${GMOCK_MAIN_LIBRARY} ${CMAKE_THREAD_LIBS_INIT} ${ISULA_LIBUTILS_LIBRARY} -lcrypto -lyajl -lz -lcurl -lselinux -larchive -lcap -ldevmapper -lhttp_parser)
add_test(NAME ${EXE} COMMAND ${EXE} --gtest_output=xml:${EXE}-Results.xml)
