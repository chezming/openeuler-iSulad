aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} thpool_src)

set(THPOOL_SRCS
    ${thpool_srcs}
    PARENT_SCOPE
    )

set(THPOOL_INCS
    ${CMAKE_CURRENT_SOURCE_DIR}
    PARENT_SCOPE
    )