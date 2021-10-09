# get current directory sources files
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} local_checkpoint_cb_srcs)

set(CHECKPOINT_CB_SRCS
	${local_checkpoint_cb_srcs}
	PARENT_SCOPE
	)

set(CHECKPOINT_CB_INCS
	${CMAKE_CURRENT_SOURCE_DIR}
	PARENT_SCOPE
	)
