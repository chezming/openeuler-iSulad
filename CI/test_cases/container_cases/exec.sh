#!/bin/bash
#
# attributes: isulad exec
# concurrent: YES
# spend time: 1

#######################################################################
##- @Copyright (C) Huawei Technologies., Ltd. 2021. All rights reserved.
# - iSulad licensed under the Mulan PSL v2.
# - You can use this software according to the terms and conditions of the Mulan PSL v2.
# - You may obtain a copy of Mulan PSL v2 at:
# -     http://license.coscl.org.cn/MulanPSL2
# - THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
# - IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
# - PURPOSE.
# - See the Mulan PSL v2 for more details.
##- @Description:CI
##- @Author: wangfengtu
##- @Create: 2021-03-09
#######################################################################

curr_path=$(dirname $(readlink -f "$0"))
data_path=$(realpath $curr_path/../data)
source ../helpers.sh
test="exec test => test_exec"

function exec_privileged()
{
    local ret=0

    isula rm -f `isula ps -a -q`

    isula run -tid -n test_privileged --cap-drop=ALL busybox sh -c 'while true;do if [ -e /exec_pri ];then mknod /tmp/sdz b 8 0 || echo "Success";fi;usleep 100000;done'
    [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - failed to run container with --cap-drop=ALL" && ((ret++))

    # make sure unprivileged user can not mknod
    isula exec -ti test_privileged mknod /tmp/sdx b 8 16
    [[ $? -eq 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - exec to mknod should fail" && ((ret++))

    # make sure privileged user can mknod
    isula exec -ti --privileged test_privileged mknod /tmp/sdx b 8 16
    [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - exec to mknod failed" && ((ret++))

    isula exec -ti test_privileged rm -f /tmp/sdx
    [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - exec to mknod should fail" && ((ret++))

    # make sure subsequent unprivileged user can not mknod
    isula exec -ti test_privileged mknod /tmp/sdx b 8 16
    [[ $? -eq 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - exec to mknod should fail" && ((ret++))

    # make sure container can not mknod after exec --privileged
    isula exec -ti test_privileged touch /exec_pri
    [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - touch /exec_pri failed" && ((ret++))

    isula exec -ti test_privileged stat /tmp/sdz
    [[ $? -eq 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - /tmp/sdz should not exist" && ((ret++))

    isula logs test_privileged | grep Success
    [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - container should mknod fail" && ((ret++))

    isula rm -f `isula ps -a -q`

    return ${ret}
}

declare -i ans=0

msg_info "${test} starting..."

exec_privileged || ((ans++))

msg_info "${test} finished with return ${ret}..."

show_result ${ans} "${curr_path}/${0}"
