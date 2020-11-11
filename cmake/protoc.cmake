set(PROTOS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/src/api/services)

set(GRPC_OUT_PRE_PATH ${CMAKE_BINARY_DIR}/grpc)
set(CONTAINER_PROTOS_OUT_PATH ${GRPC_OUT_PRE_PATH}/src/api/services/containers)
set(IMAGE_PROTOS_OUT_PATH ${GRPC_OUT_PRE_PATH}/src/api/services/images)
set(CRI_PROTOS_OUT_PATH ${GRPC_OUT_PRE_PATH}/src/api/services/cri)
set(IMAGE_SERVICE_PROTOS_OUT_PATH ${GRPC_OUT_PRE_PATH}/src/api/image_client)
set(NETWORK_PROTOS_OUT_PATH ${GRPC_OUT_PRE_PATH}/src/api/services/network)

if (GRPC_CONNECTOR)
    message("---------------Generate GRPC proto-----------------------")
    execute_process(COMMAND mkdir -p ${CONTAINER_PROTOS_OUT_PATH})
    execute_process(COMMAND mkdir -p ${IMAGE_PROTOS_OUT_PATH})
    execute_process(COMMAND mkdir -p ${CRI_PROTOS_OUT_PATH})
    execute_process(COMMAND mkdir -p ${NETWORK_PROTOS_OUT_PATH})
    execute_process(COMMAND ${CMD_PROTOC} -I ${PROTOS_PATH}/containers --cpp_out=${CONTAINER_PROTOS_OUT_PATH} 
        ${PROTOS_PATH}/containers/container.proto ERROR_VARIABLE containers_err)
    if (containers_err)
        message("Parse containers.proto failed: ")
        message(FATAL_ERROR ${containers_err})
    endif()

    execute_process(COMMAND ${CMD_PROTOC} -I ${PROTOS_PATH}/containers --grpc_out=${CONTAINER_PROTOS_OUT_PATH} --plugin=protoc-gen-grpc=${CMD_GRPC_CPP_PLUGIN} ${PROTOS_PATH}/containers/container.proto ERROR_VARIABLE containers_err)
    if (containers_err)
        message("Parse containers.proto plugin failed: ")
        message(FATAL_ERROR ${containers_err})
    endif()

    execute_process(COMMAND ${CMD_PROTOC} -I ${PROTOS_PATH}/images --cpp_out=${IMAGE_PROTOS_OUT_PATH} ${PROTOS_PATH}/images/images.proto ERROR_VARIABLE images_err)
    if (images_err)
        message("Parse images.proto failed: ")
        message(FATAL_ERROR ${images_err})
    endif()

    execute_process(COMMAND ${CMD_PROTOC} -I ${PROTOS_PATH}/images --grpc_out=${IMAGE_PROTOS_OUT_PATH} --plugin=protoc-gen-grpc=${CMD_GRPC_CPP_PLUGIN} ${PROTOS_PATH}/images/images.proto ERROR_VARIABLE images_err)
    if (images_err)
        message("Parse images.proto plugin failed: ")
        message(FATAL_ERROR ${images_err})
    endif()

    execute_process(COMMAND ${CMD_PROTOC} -I ${PROTOS_PATH}/cri --cpp_out=${CRI_PROTOS_OUT_PATH} ${PROTOS_PATH}/cri/api.proto 
        ERROR_VARIABLE cri_err)
    if (cri_err)
        message("Parse cri.proto failed: ")
        message(FATAL_ERROR ${cri_err})
    endif()

    execute_process(COMMAND ${CMD_PROTOC} -I ${PROTOS_PATH}/cri --grpc_out=${CRI_PROTOS_OUT_PATH} 
        --plugin=protoc-gen-grpc=${CMD_GRPC_CPP_PLUGIN} ${PROTOS_PATH}/cri/api.proto ERROR_VARIABLE cri_err)
    if (cri_err)
        message("Parse cri.proto plugin failed: ")
        message(FATAL_ERROR ${cri_err})
    endif()

    execute_process(COMMAND ${CMD_PROTOC} -I ${PROTOS_PATH}/network
        --cpp_out=${NETWORK_PROTOS_OUT_PATH} ${PROTOS_PATH}/network/network.proto ERROR_VARIABLE network_err)
    if (network_err)
        message("Parse network.proto failed: ")
        message(FATAL_ERROR ${network_err})
    endif()

    execute_process(COMMAND ${CMD_PROTOC} -I ${PROTOS_PATH}/network --grpc_out=${NETWORK_PROTOS_OUT_PATH}
        --plugin=protoc-gen-grpc=${CMD_GRPC_CPP_PLUGIN} ${PROTOS_PATH}/network/network.proto ERROR_VARIABLE network_err)
    if (network_err)
        message("Parse network.proto plugin failed: ")
        message(FATAL_ERROR ${network_err})
    endif()
endif()

