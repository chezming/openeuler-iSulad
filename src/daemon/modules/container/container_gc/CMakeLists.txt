# get current directory sources files
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} local_gc_srcs)

set(GC_SRCS
    ${local_gc_srcs}
    ${CMAKE_SOURCE_DIR}/src/utils/threadpool/thpool.c
    PARENT_SCOPE
    )

set(GC_INCS
    ${CMAKE_CURRENT_SOURCE_DIR}
    PARENT_SCOPE
    )
