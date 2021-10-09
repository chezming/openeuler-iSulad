
#include "checkpoint.h"

#include "isula_commands.h"
#include "isula_libutils/log.h"
#include "utils.h"
#include "cpcreate.h"

#include "cpremove.h"
#include "cplist.h"
#include "cprestore.h"

const char g_cmd_checkpoint_desc[] = "Manage checkpoints";
const char g_cmd_checkpoint_usage[] = "isula checkpoint COMMAND";


struct command g_checkpoint_commands[] = {
   {
        // `checkpoint ls` sub-command
        "ls", false, cmd_checkpoint_ls_main, g_cmd_checkpoint_ls_desc, NULL, &g_cmd_checkpoint_ls_args
    },
    
    {
        // `checkpoint create` sub-command
        "create", false, cmd_checkpoint_create_main, g_cmd_checkpoint_create_desc, NULL, &g_cmd_checkpoint_create_args
    },
    
    {
        // `checkpoint rm` sub-command
        "rm", false, cmd_checkpoint_rm_main, g_cmd_checkpoint_rm_desc, NULL, &g_cmd_checkpoint_rm_args
    },
     {
        // `checkpoint restore` sub-command
        "restore", false, cmd_checkpoint_restore_main, g_cmd_checkpoint_restore_desc, NULL, &g_cmd_checkpoint_restore_args
    },
    { NULL, false, NULL, NULL, NULL, NULL } // End of the list
};


int cmd_checkpoint_main(int argc, const char **argv)
{
    const struct command *command = NULL;
    char *program = NULL;

    program = util_string_join(" ", argv, 2);

    if (argc == 2) {
        return command_subcmd_help(program, g_checkpoint_commands, argc - 2, (const char **)(argv + 2));
    }

    if (strcmp(argv[2], "--help") == 0) {
        // isula checkpoint help command format: isula checkpoint --help
        return command_subcmd_help(program, g_checkpoint_commands, argc - 3, (const char **)(argv + 3));
    }

    command = command_by_name(g_checkpoint_commands, argv[2]);
    if (command != NULL) {
        return command->executor(argc, (const char **)argv);
    }

    printf("%s: command \"%s\" not found\n", program, argv[2]);
    printf("Run `%s --help` for a list of sub-commands\n", program);
    return 1;
}

