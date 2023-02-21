# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.22

# compile C with /usr/bin/cc
# compile CXX with /usr/bin/c++
C_DEFINES = -DEANBLE_IMAGE_LIBARAY -DENABLE_IMAGE_SEARCH -DENABLE_NATIVE_NETWORK -DENABLE_NETWORK -DENABLE_OCI_IMAGE=1 -DENABLE_SELINUX=1 -DGRPC_CONNECTOR -DISULAD_BUILD_TIME=\"2023-02-17T19:56:44.724552879+08:00\" -DISULAD_GIT_COMMIT=\"3f7fa80729ea13edae63ba379d07345251350a85\" -DISULAD_ROOT_PATH=\"/var/lib/isulad\" -DISULAD_STATE_PATH=\"/var/run/isulad\" -DOPENSSL_VERIFY -DRUNPATH=\"/var/run\" -DSYSCONFDIR_PREFIX=\"\" -DSYSTEMD_NOTIFY

C_INCLUDES = -I/root/iSulad/src/daemon -I/root/iSulad/src/daemon/executor -I/root/iSulad/src/daemon/executor/container_cb -I/root/iSulad/src/daemon/executor/image_cb -I/root/iSulad/src/daemon/executor/volume_cb -I/root/iSulad/src/daemon/executor/network_cb -I/root/iSulad/src/daemon/entry -I/root/iSulad/src/daemon/entry/connect -I/root/iSulad/src/daemon/entry/connect/grpc -I/root/iSulad/src/daemon/entry/connect/grpc/container -I/root/iSulad/test/grpc/src/api/services/containers -I/root/iSulad/test/grpc/src/api/services/images -I/root/iSulad/test/grpc/src/api/services/volumes -I/root/iSulad/test/grpc/src/api/services/cri -I/root/iSulad/test/grpc/src/api/services/network -I/root/iSulad/src/daemon/entry/cri -I/root/iSulad/src/daemon/entry/cri/websocket/service -I/root/iSulad/src/daemon/modules -I/root/iSulad/src/daemon/modules/runtime -I/root/iSulad/src/daemon/modules/runtime/engines -I/root/iSulad/src/daemon/modules/runtime/engines/lcr -I/root/iSulad/src/daemon/modules/runtime/isula -I/root/iSulad/src/daemon/modules/image -I/root/iSulad/src/daemon/modules/image/external -I/root/iSulad/src/daemon/modules/image/oci/storage -I/root/iSulad/src/daemon/modules/image/oci/storage/image_store -I/root/iSulad/src/daemon/modules/image/oci/storage/layer_store -I/root/iSulad/src/daemon/modules/image/oci/storage/layer_store/graphdriver -I/root/iSulad/src/daemon/modules/image/oci/storage/layer_store/graphdriver/overlay2 -I/root/iSulad/src/daemon/modules/image/oci/storage/layer_store/graphdriver/devmapper -I/root/iSulad/src/daemon/modules/image/oci/storage/layer_store/graphdriver/quota -I/root/iSulad/src/daemon/modules/image/oci/storage/rootfs_store -I/root/iSulad/src/daemon/modules/image/oci -I/root/iSulad/src/daemon/modules/image/oci/registry -I/root/iSulad/src/daemon/modules/plugin -I/root/iSulad/src/daemon/modules/spec -I/root/iSulad/src/daemon/modules/container -I/root/iSulad/src/daemon/modules/container/restore -I/root/iSulad/src/daemon/modules/container/supervisor -I/root/iSulad/src/daemon/modules/container/health_check -I/root/iSulad/src/daemon/modules/container/container_gc -I/root/iSulad/src/daemon/modules/container/restart_manager -I/root/iSulad/src/daemon/modules/container/leftover_cleanup -I/root/iSulad/src/daemon/modules/log -I/root/iSulad/src/daemon/modules/events -I/root/iSulad/src/daemon/modules/events_sender -I/root/iSulad/src/daemon/modules/service -I/root/iSulad/src/daemon/modules/api -I/root/iSulad/src/daemon/modules/volume -I/root/iSulad/src/daemon/config -I/root/iSulad/src/daemon/common -I/root/iSulad/src -I/root/iSulad/src/common -I/root/iSulad/src/utils -I/root/iSulad/src/utils/tar -I/root/iSulad/src/utils/sha256 -I/root/iSulad/src/utils/cutils -I/root/iSulad/src/utils/cutils/map -I/root/iSulad/src/utils/console -I/root/iSulad/src/utils/buffer -I/root/iSulad/src/utils/cpputils -I/root/iSulad/test/conf -I/root/iSulad/src/cmd -I/root/iSulad/src/cmd/options -I/root/iSulad/src/cmd/isulad -I/root/iSulad/src/daemon/modules/network -I/root/iSulad/src/daemon/modules/network/cni_operator -I/root/iSulad/src/daemon/modules/network/cni_operator/libcni -I/root/iSulad/src/daemon/modules/network/cni_operator/libcni/invoke -I/root/iSulad/src/daemon/modules/network/cri -I/root/iSulad/src/daemon/modules/network/native -I/root/iSulad/src/utils/http

C_FLAGS = -fPIC -fstack-protector-all -D_FORTIFY_SOURCE=2 -O2 -Wall -fPIE -D__FILENAME__='"$(subst /root/iSulad/,,$(abspath $<))"' -Werror -g     -g -O2

CXX_DEFINES = -DEANBLE_IMAGE_LIBARAY -DENABLE_IMAGE_SEARCH -DENABLE_NATIVE_NETWORK -DENABLE_NETWORK -DENABLE_OCI_IMAGE=1 -DENABLE_SELINUX=1 -DGRPC_CONNECTOR -DISULAD_BUILD_TIME=\"2023-02-17T19:56:44.724552879+08:00\" -DISULAD_GIT_COMMIT=\"3f7fa80729ea13edae63ba379d07345251350a85\" -DISULAD_ROOT_PATH=\"/var/lib/isulad\" -DISULAD_STATE_PATH=\"/var/run/isulad\" -DOPENSSL_VERIFY -DRUNPATH=\"/var/run\" -DSYSCONFDIR_PREFIX=\"\" -DSYSTEMD_NOTIFY

CXX_INCLUDES = -I/root/iSulad/src/daemon -I/root/iSulad/src/daemon/executor -I/root/iSulad/src/daemon/executor/container_cb -I/root/iSulad/src/daemon/executor/image_cb -I/root/iSulad/src/daemon/executor/volume_cb -I/root/iSulad/src/daemon/executor/network_cb -I/root/iSulad/src/daemon/entry -I/root/iSulad/src/daemon/entry/connect -I/root/iSulad/src/daemon/entry/connect/grpc -I/root/iSulad/src/daemon/entry/connect/grpc/container -I/root/iSulad/test/grpc/src/api/services/containers -I/root/iSulad/test/grpc/src/api/services/images -I/root/iSulad/test/grpc/src/api/services/volumes -I/root/iSulad/test/grpc/src/api/services/cri -I/root/iSulad/test/grpc/src/api/services/network -I/root/iSulad/src/daemon/entry/cri -I/root/iSulad/src/daemon/entry/cri/websocket/service -I/root/iSulad/src/daemon/modules -I/root/iSulad/src/daemon/modules/runtime -I/root/iSulad/src/daemon/modules/runtime/engines -I/root/iSulad/src/daemon/modules/runtime/engines/lcr -I/root/iSulad/src/daemon/modules/runtime/isula -I/root/iSulad/src/daemon/modules/image -I/root/iSulad/src/daemon/modules/image/external -I/root/iSulad/src/daemon/modules/image/oci/storage -I/root/iSulad/src/daemon/modules/image/oci/storage/image_store -I/root/iSulad/src/daemon/modules/image/oci/storage/layer_store -I/root/iSulad/src/daemon/modules/image/oci/storage/layer_store/graphdriver -I/root/iSulad/src/daemon/modules/image/oci/storage/layer_store/graphdriver/overlay2 -I/root/iSulad/src/daemon/modules/image/oci/storage/layer_store/graphdriver/devmapper -I/root/iSulad/src/daemon/modules/image/oci/storage/layer_store/graphdriver/quota -I/root/iSulad/src/daemon/modules/image/oci/storage/rootfs_store -I/root/iSulad/src/daemon/modules/image/oci -I/root/iSulad/src/daemon/modules/image/oci/registry -I/root/iSulad/src/daemon/modules/plugin -I/root/iSulad/src/daemon/modules/spec -I/root/iSulad/src/daemon/modules/container -I/root/iSulad/src/daemon/modules/container/restore -I/root/iSulad/src/daemon/modules/container/supervisor -I/root/iSulad/src/daemon/modules/container/health_check -I/root/iSulad/src/daemon/modules/container/container_gc -I/root/iSulad/src/daemon/modules/container/restart_manager -I/root/iSulad/src/daemon/modules/container/leftover_cleanup -I/root/iSulad/src/daemon/modules/log -I/root/iSulad/src/daemon/modules/events -I/root/iSulad/src/daemon/modules/events_sender -I/root/iSulad/src/daemon/modules/service -I/root/iSulad/src/daemon/modules/api -I/root/iSulad/src/daemon/modules/volume -I/root/iSulad/src/daemon/config -I/root/iSulad/src/daemon/common -I/root/iSulad/src -I/root/iSulad/src/common -I/root/iSulad/src/utils -I/root/iSulad/src/utils/tar -I/root/iSulad/src/utils/sha256 -I/root/iSulad/src/utils/cutils -I/root/iSulad/src/utils/cutils/map -I/root/iSulad/src/utils/console -I/root/iSulad/src/utils/buffer -I/root/iSulad/src/utils/cpputils -I/root/iSulad/test/conf -I/root/iSulad/src/cmd -I/root/iSulad/src/cmd/options -I/root/iSulad/src/cmd/isulad -I/root/iSulad/src/daemon/modules/network -I/root/iSulad/src/daemon/modules/network/cni_operator -I/root/iSulad/src/daemon/modules/network/cni_operator/libcni -I/root/iSulad/src/daemon/modules/network/cni_operator/libcni/invoke -I/root/iSulad/src/daemon/modules/network/cri -I/root/iSulad/src/daemon/modules/network/native -I/root/iSulad/src/utils/http

CXX_FLAGS = -fPIC -std=c++11 -fstack-protector-all -D_FORTIFY_SOURCE=2 -O2 -Wall -Wno-error=deprecated-declarations -D__FILENAME__='"$(subst /root/iSulad/,,$(abspath $<))"' -Werror -g     -g -O2 -std=c++14

