# get current directory sources files
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} local_devmapper_srcs)

set(DEVMAPPER_SRCS
    ${local_devmapper_srcs}
    ${CMAKE_SOURCE_DIR}/src/utils/threadpool/thpool.c
    PARENT_SCOPE
    )
