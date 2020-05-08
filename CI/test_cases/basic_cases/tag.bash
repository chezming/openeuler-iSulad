#!/bin/bash
#
# attributes: isulad inheritance tag
# concurrent: YES
# spend time: 14

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
##- @Author: jikui
##- @Create: 2020-05-05
#######################################################################

curr_path=$(dirname $(readlink -f "$0"))
data_path=$(realpath $curr_path/../data)
source ./helpers.bash

image_busybox="busybox"
image_ubuntu="ubuntu"

function make_environment()
{
    isula inspect -f f '{{json .image.repo_tags}}' $image_busybox >/dev/null 2>&1
    if [[ $? -ne 0 ]];then
        isula pull $image_busybox
    fi

    isula inspect -f f '{{json .image.repo_tags}}' $image_ubuntu >/dev/null 2>&1
    if [[ $? -ne 0 ]];then
        isula pull $image_ubuntu
    fi
}
function tag_exist_image()
{
    isula tag $image_busybox "aaa"
    fn_check_eq $? 0 "image tag failed"

    isula inspect -f '{{json .image.repo_tags}}' $image_busybox|grep "aaa" >/dev/null 2>&1
    fn_check_eq $? 0 "image tag being writen"

    isula rmi  "aaa"
}

function tag_not_exist_image()
{
    isula tag "image_not_exist" "aaa" >/dev/null 2>&1
    fn_check_ne $? 0 "image tag should failed"
}

function tag_use_TAG()
{
    isula tag $image_busybox "aaa:bbb"
    fn_check_eq $? 0 "image tag failed"
    isula inspect -f '{{json .image.repo_tags}}' $image_busybox|grep "aaa:bbb" >/dev/null 2>&1
    fn_check_eq $? 0 "isula inspect failed"
    isula rmi  "aaa:bbb"
}

function tag_name_being_used()
{
    ID_first=`isula inspect -f '{{json .image.id}}' $image_busybox`
    ID_second=`isula inspect -f '{{json .image.id}}' $image_ubuntu`

    isula tag $image_busybox "aaa"
    ID_before=`isula inspect -f '{{json .image.id}}' "aaa"`
    if [[ $ID_first != $ID_before ]];then
        fn_check_eq 1 2 "wrong image ID"
    fi
    
    isula tag $image_ubuntu "aaa"
    ID_after=`isula inspect -f '{{json .image.id}}' "aaa"`
    if [[ $ID_second != $ID_after ]];then
        fn_check_eq 1 2 "wrong image ID"
    fi

    isula rmi  "aaa"
}

function do_test_t()
{
    tag_exist_image
    tag_not_exist_image
    tag_use_TAG
    tag_name_being_used

    return $TC_RET_T
}

ret=0
make_environment
do_test_t
if [ $? -ne 0 ];then
    let "ret=$ret + 1"
fi

show_result $ret "basic tag"
