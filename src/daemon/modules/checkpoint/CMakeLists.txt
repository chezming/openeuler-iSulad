# get current directory sources files
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} local_checkpoint_srcs)

set(CHECKPOINT_SRCS
	${local_checkpoint_srcs}
	PARENT_SCOPE
	)

set(CHECKPOINT_INCS
	${CMAKE_CURRENT_SOURCE_DIR}
	PARENT_SCOPE
	)