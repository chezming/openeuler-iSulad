#!/bin/bash
#
# attributes: test domainname
# concurrent: YES
# spend time: 2

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
##- @Create: 2021-02-26
#######################################################################

declare -r curr_path=$(dirname $(readlink -f "$0"))
source ../helpers.sh
test="domainname test => test_domainname"

function test_domainname()
{
  local ret=0

  # set sysctl kernel.domainname
  isula run --rm --sysctl kernel.domainname=test.com busybox sh
  [[ $? -eq 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - set sysctl kernel.domainname should failed" && ((ret++))

  # uts conflict with domainname
  isula run --rm --uts=host --domainname=test.com busybox sh
  [[ $? -eq 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - uts conflict with domainname should failed" && ((ret++))

  # test --domainname
  isula run -tid -n domainname --hostname=hostname --domainname=domainname.com busybox sh
  [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - test domainname failed" && ((ret++))

  isula exec -ti domainname cat /proc/sys/kernel/hostname | grep hostname
  [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - hostname not right failed" && ((ret++))

  isula exec -ti domainname cat /proc/sys/kernel/domainname | grep domainname.com
  [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - hostname not right failed" && ((ret++))

  isula rm -f domainname
  [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - remove container with domainname failed" && ((ret++))

  return ${ret}
}

declare -i ans=0

msg_info "${test} starting..."
[[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - start isulad failed" && ((ret++))

test_domainname || ((ans++))

msg_info "${test} finished with return ${ans}..."

show_result ${ans} "${curr_path}/${0}"
