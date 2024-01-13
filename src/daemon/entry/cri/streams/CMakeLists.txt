# get current directory sources files
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} cri_stream_srcs)

add_subdirectory(websocket)

set(cri_stream_incs ${CMAKE_CURRENT_SOURCE_DIR})

if(NOT ENABLE_PORTFORWARD)
    list(REMOVE_ITEM cri_stream_incs "${CMAKE_CURRENT_SOURCE_DIR}/portforward_serve.h")
endif()

if(NOT ENABLE_PORTFORWARD)
    list(REMOVE_ITEM cri_stream_srcs "${CMAKE_CURRENT_SOURCE_DIR}/portforward_serve.cc")
endif()

set(STREAM_SERVICE_SRCS
    ${cri_stream_srcs}
    ${WB_SERVICE_SRCS}
    PARENT_SCOPE)

set(STREAM_SERVICE_INCS
    ${cri_stream_incs}
    ${WB_SERVICE_INCS}
    PARENT_SCOPE)
