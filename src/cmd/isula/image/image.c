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
#include <isula_libutils/auto_cleanup.h>
#include <isula_libutils/log.h>
#include "isula_commands.h"
#include "image.h"
#include "prune.h"
#include "utils.h"

struct command g_image_commands[] = {
    {
        // `image prune` sub-command
        "prune", false, cmd_image_prune_main, g_cmd_image_prune_desc, NULL, &g_cmd_image_prune_args
    },
    { NULL, NULL, NULL, NULL, NULL } // End of the list
};

const char g_cmd_image_desc[] = "Manage images";
const char g_cmd_image_usage[] = "isula image COMMAND";

int cmd_image_main(int argc, const char **argv)
{
    const struct command *command = NULL;
    __isula_auto_free char *program = NULL;

    program = util_string_join(" ", argv, 2);

    if (argc == 2) {
        return command_subcmd_help(program, g_image_commands, argc - 2, (const char **)(argv + 2));
    }

    if (strcmp(argv[2], "--help") == 0) {
        // isula image help command format: isula image --help args
        return command_subcmd_help(program, g_image_commands, argc - 3, (const char **)(argv + 3));
    }

    command = command_by_name(g_image_commands, argv[2]);
    if (command != NULL) {
        return command->executor(argc, (const char **)argv);
    }

    printf("%s: command \"%s\" not found\n", program, argv[2]);
    printf("Run `%s --help` for a list of sub-commands\n", program);
    return 1;
}
