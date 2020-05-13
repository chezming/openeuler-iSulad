#!/bin/bash
#
# attributes: isulad inheritance version
# concurrent: YES
# spend time: 1

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
##- @Create: 2020-05-04
#######################################################################

curr_path=$(dirname $(readlink -f "$0"))
data_path=$(realpath $curr_path/../data)
driver="overlay2"
source ./helpers.bash
data_set="LowerDir MergedDir UpperDir WorkDir DeviceId DeviceName DeviceSize"

do_test_t()
{
	id=`isula run -tid busybox`
	fn_check_eq "$?" "0" "run failed"
	testcontainer $id running

	cut_output_lines isula inspect -format='{{json .GraphDriver.Data}}' $id
	fn_check_eq "$?" "0" "check failed"
	fn_check_eq "${#lines[@]}" "7" "graph driver data failed"

	for i in ${lines[@]};do
		data=`echo $i | awk -F ':' '{print $1}'`
		if ! [[ $data_set =~ $data ]];then 
			echo "Graph Data do not contain $data"
			TC_RET_T=$(($TC_RET_T+1))
		done
	done

	
	driver_name=`sula inspect --format='{{json .GraphDriver.Name}}' $id`
	fn_check_eq "$?" "0" "inspect container $id failed"

	if ! [[ "${driver_name}" =~ "^overlay" ]] && ! [[ "${driver_name}" = "devicemapper" ]];then
		echo "expect GraphDriver Name is overlay or devicemapper, not ${driver_name}"
		TC_RET_T=$(($TC_RET_T+1))
	fi

	isula rm -f $id
	fn_check_eq "$?" "0" "rm failed"

	return $TC_RET_T

}

ret = 0

do_test_t
if [ $? -ne 0 ];then
	let "ret=$ret + 1"
fi

show_result $ret "basic storage metadata"
