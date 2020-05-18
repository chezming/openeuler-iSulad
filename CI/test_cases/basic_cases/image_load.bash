#!/bin/bash
#
# attributes: isulad basic image
# concurrent: NA
# spend time: 2

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
##- @Author: gaohuatao
##- @Create: 2020-05-14
#######################################################################


curr_path=$(dirname $(readlink -f "$0"))
data_path=$(realpath $curr_path/../data)
source ./helpers.bash
data="${curr_path}/busybox.tar"
name="rnd-dockerhub.huawei.com/official/busybox"

do_test_t()
{
  echo "test begin"
  isula load -i $data 
  if [ $? -ne 0 ]; then
    echo "failed load image"
    TC_RET_T=$(($TC_RET_T+1))
  fi

  isula images | grep $name
  if [ $? -ne 0 ]; then
    echo "add image $name failed"
    TC_RET_T=$(($TC_RET_T+1))
  fi

  isula rmi $name
  if [ $? -ne 0 ]; then
    echo "Failed to remove image $name"
    TC_RET_T=$(($TC_RET_T+1))
  fi

  echo "test end"
  return $TC_RET_T
}

ret=0

do_test_t
if [ $? -ne 0 ];then
  let "ret=$ret + 1"
fi

show_result $ret "image_load.bash"
