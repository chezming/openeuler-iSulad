# get current directory sources files
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} isula_checkpoint_srcs)

set(ISULA_CHECKPOINT_SRCS
    ${isula_checkpoint_srcs}
    PARENT_SCOPE
    )
