#!/bin/bash
#
# attributes: container network cleanup
# concurrent: NA
# spend time: 118

#######################################################################
##- @Copyright (C) Huawei Technologies., Ltd. 2020. All rights reserved.
# - iSulad licensed under the Mulan PSL v2.
# - You can use this software according to the terms and conditions of the Mulan PSL v2.
# - You may obtain a copy of Mulan PSL v2 at:
# -     http://license.coscl.org.cn/MulanPSL2
# - THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
# - IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
# - PURPOSE.
# - See the Mulan PSL v2 for more details.
##- @Description:CI
##- @Author: zhangxiaoyu
##- @Create: 2021-02-04
#######################################################################

curr_path=$(dirname $(readlink -f "$0"))
source ../helpers.sh

function test_container_network_cleanup()
{
    local ret=0
    local cont_name="cont_name"

    local test="container network cleanup test => (${FUNCNAME[@]})"

    msg_info "${test} starting..."

    check_valgrind_log
    [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - stop isulad failed" && ((ret++))

    start_isulad_with_valgrind
    [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - start isulad failed" && ((ret++))

    # run container with network
    for i in $(seq 1 10); do
        isula run -tid -n ${cont_name}${i} busybox sh
        [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - run container ${cont_name}${i} failed" && return ${FAILURE}
    done

    # kill lxc
    pkill lxc-start
    [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - kill lxc-start failed" && return ${FAILURE}

    for i in $(seq 1 10); do
        for j in $(seq 1 20); do
            status=$(isula inspect -f {{.State.Status}} ${cont_name}${i})
            if [ "x${status}" == "xexited" ]; then
                break;
            fi
            sleep 1
        done

        status=$(isula inspect -f {{.State.Status}} ${cont_name}${i})
        [[ "x${status}" != "xexited" ]] && msg_err "${FUNCNAME[0]}:${LINENO} - kill container ${cont_name}${i} failed" && return ${FAILURE}
    done
    
    # test iptables rule cleanup
    for i in $(seq 1 10); do
        cont_id=$(isula inspect -f {{.Id}} ${cont_name}${i})
        [[ "x${cont_id}" == "x" ]] && msg_err "${FUNCNAME[0]}:${LINENO} - get container ${cont_name}${i} Id failed" && return ${FAILURE}
    
        isula start ${cont_name}${i}
        [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - start container ${cont_name}${i} failed" && return ${FAILURE}

        isula stop -t 0 ${cont_name}${i}
        [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - stop container ${cont_name}${i} failed" && return ${FAILURE}

        iptables -t nat --list --wait | grep ${cont_id}
        [[ $? -eq 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - inspect iptables rules success after stop container" && return ${FAILURE}

        ip6tables -t nat --list --wait | grep ${cont_id}
        [[ $? -eq 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - inspect ip6tables rules success after stop container" && return ${FAILURE}
    done
    
    for i in $(seq 1 10); do
        isula start ${cont_name}${i}
        [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - start container ${cont_name}${i} failed" && return ${FAILURE}
    done

    # stop isulad and kill lxc
    check_valgrind_log
    [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - stop isulad failed" && ((ret++))

    pkill lxc-start
    [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - kill lxc-start failed" && return ${FAILURE}

    start_isulad_with_valgrind
    [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - start isulad failed" && ((ret++))

    # test iptables rule cleanup
    for i in $(seq 1 10); do
        cont_id=$(isula inspect -f {{.Id}} ${cont_name}${i})
        [[ "x${cont_id}" == "x" ]] && msg_err "${FUNCNAME[0]}:${LINENO} - get container ${cont_name}${i} Id failed" && return ${FAILURE}

        for j in $(seq 1 20); do
            isula start ${cont_name}${i}
            if [ $? -eq 0 ]; then
                break;
            fi
            sleep 2
        done

        status=$(isula inspect -f {{.State.Status}} ${cont_name}${i})
        [[ "x${status}" != "xrunning" ]] && msg_err "${FUNCNAME[0]}:${LINENO} - start container ${cont_name}${i} failed after retry" && return ${FAILURE}

        isula stop -t 0 ${cont_name}${i}
        [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - stop container ${cont_name}${i} failed" && return ${FAILURE}

        iptables -t nat --list --wait | grep ${cont_id}
        [[ $? -eq 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - inspect iptables rules success after stop container" && return ${FAILURE}

        ip6tables -t nat --list --wait | grep ${cont_id}
        [[ $? -eq 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - inspect ip6tables rules success after stop container" && return ${FAILURE}
    done

    isula rm $(isula ps -aq)
    [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - rm containers failed" && return ${FAILURE}

    check_valgrind_log
    [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - stop isulad failed" && ((ret++))

    start_isulad_with_valgrind
    [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - start isulad failed" && ((ret++))

    msg_info "${test} finished with return ${ret}..."
    return ${ret}
}

declare -i ans=0

test_container_network_cleanup || ((ans++))

show_result ${ans} "${curr_path}/${0}"
