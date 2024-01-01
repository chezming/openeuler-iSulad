# get current directory sources files
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} top_srcs)

set(CMD_ISULAD_SRCS ${top_srcs}
    ${CMAKE_SOURCE_DIR}/src/utils/threadpool/thpool.c
    PARENT_SCOPE)
set(CMD_ISULAD_INCS ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)
