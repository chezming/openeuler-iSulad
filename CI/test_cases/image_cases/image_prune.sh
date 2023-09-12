#!/bin/bash
#
# attributes: isulad image prune
# concurrent: NA
# spend time: 

#######################################################################
##- Copyright (c) China Unicom Technologies Co., Ltd. 2023-2033. All rights reserved.
# - iSulad licensed under the Mulan PSL v2.
# - You can use this software according to the terms and conditions of the Mulan PSL v2.
# - You may obtain a copy of Mulan PSL v2 at:
# -     http://license.coscl.org.cn/MulanPSL2
# - THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
# - IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
# - PURPOSE.
# - See the Mulan PSL v2 for more details.
##- @Description:CI
##- @Author: Chen Wei
##- @Create: 2023-08-17
#######################################################################

declare -r curr_path=$(dirname $(readlink -f "$0"))
source ../helpers.sh
busybox_image="${curr_path}/busybox.tar"
nginx_image="${curr_path}/nginx.tar"

function prepare()
{
	isula load -i $busybox_image
	isula load -i $nginx_image --tag busybox:latest
}

function clean()
{
	isula images | awk '{print $3}' | grep -v IMAGE | xargs isula rmi
}

function test_image_prune()
{
  local ret=0
  local test="isula image prune test => (${FUNCNAME[@]})"

  msg_info "${test} starting..."

  # 1. no image exists, expect success
  isula image prune -f
  if [ $? -ne 0 ]
  then
    msg_err "${FUNCNAME[0]}:${LINENO} - image prune no image test failed"
	$ret++
  else
	msg_info "image prune no image test success"
  fi

  # 2. prune --all, expect no image left
  prepare
  isula image prune --all -f
  output=`isula images | awk '{print $3}' | grep -v "IMAGE" | wc -l`
  if [ $output != "0" ]
  then
    msg_err "${FUNCNAME[0]}:${LINENO} - image prune --all test failed: $output"
	((ret++))
  else
	msg_info "image prune --all test success"
  fi
  clean

  # 3. prune, expect only dangling images are deleted
  prepare
  isula image prune -f
  output=`isula images | awk '{print $3}' | grep -v "IMAGE" | wc -l`
  if [ $output != "1" ]
  then
    msg_err "${FUNCNAME[0]}:${LINENO} - image prune test failed: $output"
	((ret++))
  else
	msg_info "image prune test success"
  fi
  clean

  # 4. prune --filter before, expect all images are deleted
  prepare
  isula image prune --filter until="1970-01-30T00:00:00" --all -f
  output=`isula images | awk '{print $3}' | grep -v "IMAGE" | wc -l`
  if [ $output != "0" ]
  then
    msg_err "${FUNCNAME[0]}:${LINENO} - image prune filter before test failed: $output"
	((ret++))
  else
	msg_info "image prune filter before test success"
  fi
  clean

  # 5. prune --filter after, expect no images is deleted
  prepare
  isula image prune --filter until="2024-01-30T00:00:00" --all -f
  output=`isula images | awk '{print $3}' | grep -v "IMAGE" | wc -l`
  if [ $output != "2" ]
  then
    msg_err "${FUNCNAME[0]}:${LINENO} - image prune filter after test failed: $output"
	((ret++))
  else
	msg_info "image prune filter after test success"
  fi
  clean

  msg_info "${test} finished with return ${ret}..."
  return ${ret}
}

declare -i ans=0

test_image_prune || ((ans++))

show_result ${ans} "${curr_path}/${0}"

