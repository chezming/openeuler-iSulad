# get current directory sources files
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} isula_image_srcs)

set(ISULA_IMAGE_SRCS
    ${isula_image_srcs}
    PARENT_SCOPE
    )
