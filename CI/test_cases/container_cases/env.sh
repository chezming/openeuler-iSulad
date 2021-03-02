#!/bin/bash
#
# attributes: test env
# concurrent: YES
# spend time: 6

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
##- @Author: wangfengtu
##- @Create: 2021-03-02
#######################################################################

declare -r curr_path=$(dirname $(readlink -f "$0"))
source ../helpers.sh
test="env test => test_env"

function test_run_env()
{
  local ret=0

  # test run one env
  isula run --rm -ti -e ENV_AAA=env_aaa busybox env | grep "ENV_AAA=env_aaa"
  [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - test run one env failed" && ((ret++))

  # test run env space
  isula run --rm -ti -e ENV_SPACE="env space" busybox env | grep "ENV_SPACE=env space"
  [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - test run env space failed" && ((ret++))

  # test run two env aaa
  isula run --rm -ti -e ENV_AAA=env_aaa -e ENV_BBB=env_bbb busybox env | grep "ENV_AAA=env_aaa"
  [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - test run two env aaa failed" && ((ret++))

  # test run two env bbb
  isula run --rm -ti -e ENV_AAA=env_aaa -e ENV_BBB=env_bbb busybox env | grep "ENV_BBB=env_bbb"
  [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - test run two env bbb failed" && ((ret++))

  # test run env-file one env
  echo "ENV_FILE_AAA=env_file_aaa" > /tmp/test_run_env_file
  isula run --rm -ti --env-file /tmp/test_run_env_file busybox env | grep "ENV_FILE_AAA=env_file_aaa"
  [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - test run env-file one env failed" && ((ret++))

  # test run env-file space
  echo "ENV_FILE_SPACE=env_file space" > /tmp/test_run_env_file
  isula run --rm -ti --env-file /tmp/test_run_env_file busybox env | grep "ENV_FILE_SPACE=env_file space"
  [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - test run env-file space failed" && ((ret++))

  # test run env-file aaa
  echo "ENV_FILE_AAA=env_file_aaa\nENV_FILE_BBB=env_file_bbb" > /tmp/test_run_env_file
  isula run --rm -ti --env-file /tmp/test_run_env_file busybox env | grep "ENV_FILE_AAA=env_file_aaa"
  [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - test run env-file aaa failed" && ((ret++))

  # test run env-file bbb
  isula run --rm -ti --env-file /tmp/test_run_env_file busybox env | grep "ENV_FILE_BBB=env_file_bbb"
  [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - test run env-file bbb failed" && ((ret++))

  # test run env conflict
  echo "ENV_CONFLICT=conflict" > /tmp/test_run_env_file
  isula run --rm -ti --env-file /tmp/test_run_env_file --env ENV_CONFLICT=aaa busybox env | grep "ENV_CONFLICT=aaa"
  [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - test run conflict failed" && ((ret++))

  rm -f /tmp/test_run_env_file

  return ${ret}
}

function test_exec_env()
{
  local ret=0

  isula run -tid -n con_exec -e RUN_ENV=run_env busybox sh
  [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - test exec for exec env failed" && ((ret++))

  # test exec one env
  isula exec -ti -e ENV_AAA=env_aaa con_exec env | grep "ENV_AAA=env_aaa"
  [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - test exec one env failed" && ((ret++))

  # test exec env space
  isula exec -ti -e ENV_SPACE="env space" con_exec env | grep "ENV_SPACE=env space"
  [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - test exec env space failed" && ((ret++))

  # test exec two env aaa
  isula exec -ti -e ENV_AAA=env_aaa -e ENV_BBB=env_bbb con_exec env | grep "ENV_AAA=env_aaa"
  [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - test exec two env aaa failed" && ((ret++))

  # test exec two env bbb
  isula exec -ti -e ENV_AAA=env_aaa -e ENV_BBB=env_bbb con_exec env | grep "ENV_BBB=env_bbb"
  [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - test exec two env bbb failed" && ((ret++))

  # test exec env-file one env
  echo "ENV_FILE_AAA=env_file_aaa" > /tmp/test_exec_env_file
  isula exec -ti --env-file /tmp/test_exec_env_file con_exec env | grep "ENV_FILE_AAA=env_file_aaa"
  [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - test exec env-file one env failed" && ((ret++))

  # test exec env-file space
  echo "ENV_FILE_SPACE=env_file space" > /tmp/test_exec_env_file
  isula exec -ti --env-file /tmp/test_exec_env_file con_exec env | grep "ENV_FILE_SPACE=env_file space"
  [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - test exec env-file space failed" && ((ret++))

  # test exec env-file aaa
  echo "ENV_FILE_AAA=env_file_aaa\nENV_FILE_BBB=env_file_bbb" > /tmp/test_exec_env_file
  isula exec -ti --env-file /tmp/test_exec_env_file con_exec env | grep "ENV_FILE_AAA=env_file_aaa"
  [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - test exec env-file aaa failed" && ((ret++))

  # test exec env-file bbb
  isula exec -ti --env-file /tmp/test_exec_env_file con_exec env | grep "ENV_FILE_BBB=env_file_bbb"
  [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - test exec env-file bbb failed" && ((ret++))

  # test exec env conflict
  echo "ENV_CONFLICT=conflict" > /tmp/test_exec_env_file
  isula exec -ti --env-file /tmp/test_exec_env_file --env ENV_CONFLICT=aaa con_exec env | grep "ENV_CONFLICT=aaa"
  [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - test exec conflict failed" && ((ret++))

  # test conflict with run env
  isula exec -ti --env RUN_ENV=conflict con_exec env | grep "RUN_ENV=conflict"
  [[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - test conflict with run env failed" && ((ret++))

  rm -f /tmp/test_exec_env_file

  isula rm -f con_exec

  return ${ret}
}

declare -i ans=0

msg_info "${test} starting..."
[[ $? -ne 0 ]] && msg_err "${FUNCNAME[0]}:${LINENO} - start isulad failed" && ((ret++))

test_run_env || ((ans++))
test_exec_env || ((ans++))

msg_info "${test} finished with return ${ans}..."

show_result ${ans} "${curr_path}/${0}"
