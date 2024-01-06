# get current directory sources files
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} local_restart_manager_srcs)

set(RESTART_MANAGER_SRCS
    ${local_restart_manager_srcs}
    ${CMAKE_SOURCE_DIR}/src/utils/threadpool/thpool.c
    PARENT_SCOPE
    )

set(RESTART_MANAGER_INCS
    ${CMAKE_CURRENT_SOURCE_DIR}
    PARENT_SCOPE
    )
