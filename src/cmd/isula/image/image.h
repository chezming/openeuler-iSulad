/******************************************************************************
 * Copyright (c) China Unicom Technologies Co., Ltd. 2023-2033. All rights reserved.
 *
 * iSulad licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 *
 * Author: Chen Wei
 * Create: 2023-05-11
 * Description: management command "isula image"
 ******************************************************************************/
#ifndef CMD_ISULA_IMAGE_H
#define CMD_ISULA_IMAGE_H

#ifdef __cplusplus
extern "C" {
#endif

extern const char g_cmd_image_desc[];
int cmd_image_main(int argc, const char **argv);

#ifdef __cplusplus
}
#endif

#endif // CMD_ISULA_IMAGE_H
