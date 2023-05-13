#!/bin/bash
#
# attributes: isulad inheritance update
# concurrent: YES
# spend time: 16

#######################################################################
##- Copyright (c) Huawei Technologies Co., Ltd. 2020. All rights reserved.
# - iSulad licensed under the Mulan PSL v2.
# - You can use this software according to the terms and conditions of the Mulan PSL v2.
# - You may obtain a copy of Mulan PSL v2 at:
# -     http://license.coscl.org.cn/MulanPSL2
# - THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
# - IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
# - PURPOSE.
# - See the Mulan PSL v2 for more details.
##- @Description:CI
##- @Author: lifeng
##- @Create: 2020-03-30
#######################################################################

curr_path=$(dirname $(readlink -f "$0"))
data_path=$(realpath $curr_path/../data)
source ../helpers.sh

function do_test_t()
{
    containername=test_update
    containerid=`isula create -t --runtime $1 --name $containername busybox`
    fn_check_eq "$?" "0" "create failed"
    testcontainer $containername inited

    isula start $containername
    fn_check_eq "$?" "0" "start failed"
    testcontainer $containername running

    isula update --kernel-memory 1000000000 $containername
    fn_check_ne "$?" "0" "update should fail"

    isula update --cpu-shares 1024 $containername
    fn_check_eq "$?" "0" "update failed"
    tmp=`isula exec $containername cat /sys/fs/cgroup/cpu/cpu.shares`
    if [ $tmp != "1024" ]; then
        echo "Failed to check running cpu shares"
        TC_RET_T=$(($TC_RET_T+1))
    fi

    isula update --cpu-period 100000 $containername
    fn_check_eq "$?" "0" "update failed"
    tmp=`isula exec $containername cat /sys/fs/cgroup/cpu/cpu.cfs_period_us`
    if [ $tmp != "100000" ]; then
        echo "Failed to check running cpu period"
        TC_RET_T=$(($TC_RET_T+1))
    fi

    isula update --cpu-quota 50000 $containername
    fn_check_eq "$?" "0" "update failed"
    tmp=`isula exec $containername cat /sys/fs/cgroup/cpu/cpu.cfs_quota_us`
    if [ $tmp != "50000" ]; then
        echo "Failed to check running cpu quota"
        TC_RET_T=$(($TC_RET_T+1))
    fi

    isula update --cpuset-cpus 0-1 $containername
    fn_check_eq "$?" "0" "update failed"
    tmp=`isula exec $containername cat /sys/fs/cgroup/cpuset/cpuset.cpus`
    if [ $tmp != "0-1" ]; then
        echo "Failed to check running cpu set"
        TC_RET_T=$(($TC_RET_T+1))
    fi

    isula update --cpuset-mems 0 $containername
    fn_check_eq "$?" "0" "update failed"
    tmp=`isula exec $containername cat /sys/fs/cgroup/cpuset/cpuset.mems`
    if [ $tmp != "0" ]; then
        echo "Failed to check running cpu set mems"
        TC_RET_T=$(($TC_RET_T+1))
    fi

    isula update --memory 1000000000 $containername
    fn_check_eq "$?" "0" "update failed"
    tmp=`isula exec $containername cat /sys/fs/cgroup/memory/memory.limit_in_bytes`
    value=$(($tmp - 1000000000))
    if [[ $value -lt -10000 || $value -gt 10000 ]]; then
        echo "Failed to check running memory"
        TC_RET_T=$(($TC_RET_T+1))
    fi

    isula update --memory-reservation 100000000 $containername
    fn_check_eq "$?" "0" "update failed"
    tmp=`isula exec $containername cat /sys/fs/cgroup/memory/memory.soft_limit_in_bytes`
    value=$(($tmp - 100000000))
    if [[ $value -lt -10000 || $value -gt 10000 ]]; then
        echo "Failed to check running reservation memory"
        TC_RET_T=$(($TC_RET_T+1))
    fi

    isula stop -t 0 $containername
    fn_check_eq "$?" "0" "stop failed"
    testcontainer $containername exited

    main=$(uname -r | awk -F . '{print $1}')
    minor=$(uname -r | awk -F . '{print $2}')
    enable=1
    if [ $1 == "runc" ]; then
        version=$(runc --version | grep runc)
        # Runc does not support '--kernel-memory' options from v1.0.0-rc94 version
        limit=(1 0 0 93)
        array=`echo $version |egrep -o "[0-9]*"`
        index=0
        for i in $(echo $array| awk '{print $1,$2}')
        do
            echo $i
            if [[ $i -gt ${limit[index]} ]]; then
                    enable=0
                    break
            fi
            let "index+=1"
        done
    fi
    if [[ ${main} -lt 5 ]] || [[ ${main} -eq 5 ]] && [[ ${minor} -lt 11 ]] && [[ ${enable} -eq 1 ]]; then
        isula update --kernel-memory 2000000000 $containername
        fn_check_eq "$?" "0" "update failed"

        isula start $containername
        fn_check_eq "$?" "0" "start failed"
        tmp=`isula exec $containername cat /sys/fs/cgroup/memory/memory.kmem.limit_in_bytes`
        value=$(($tmp - 2000000000))
        if [[ $value -lt -10000 || $value -gt 10000 ]]; then
            echo "Failed to check running reservation memory"
            TC_RET_T=$(($TC_RET_T+1))
        fi
    fi

    isula rm -f $containername
    fn_check_eq "$?" "0" "rm failed"

    return $TC_RET_T
}

function do_test_t1()
{
    containername=test_update1
    containerid=`isula run -itd --runtime $1 --memory 500M --name $containername busybox`
    fn_check_eq "$?" "0" "run failed"

    isula inspect $containerid | grep "MemorySwap" | grep "1048576000"
    fn_check_eq "$?" "0" "inspect memory swap failed"

    isula update --memory 2G $containername > /tmp/test_update1.log 2>&1
    fn_check_eq "$?" "1" "Success update memory with 2G, expect fail"

    cat /tmp/test_update1.log | grep "Memory limit should be smaller than already set memoryswap limit, update the memoryswap at the same time."
    fn_check_eq "$?" "0" "Failed to check error message"

    rm -rf /tmp/test_update1.log

    isula rm -f $containername
    fn_check_eq "$?" "0" "rm failed"

    return $TC_RET_T
}

ret=0

for element in ${RUNTIME_LIST[@]};
do
    test="update test => (${element})"
    msg_info "${test} starting..."

    do_test_t $element
    if [ $? -ne 0 ];then
        let "ret=$ret + 1"
    fi

    if [ -f "/sys/fs/cgroup/memory/memory.memsw.usage_in_bytes" ];then
        do_test_t1 $element
        if [ $? -ne 0 ];then
            let "ret=$ret + 1"
        fi
    fi
    msg_info "${test} finished with return ${ret}..."
done

show_result $ret "basic update"
