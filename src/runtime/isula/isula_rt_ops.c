#define _GNU_SOURCE

#include <unistd.h>
#include <sys/wait.h>

#include <limits.h>
#include "isula_rt_ops.h"
#include "log.h"
#include "error.h"
#include "runtime.h"
#include "engine.h"
#include "constants.h"
#include "shim_client_process_state.h"
#include "oci_runtime_state.h"
#include "isulad_config.h"

#define SHIM_BINARY "isulad-shim"
#define PID_WAIT_TIME 120

static void copy_process(shim_client_process_state *p, defs_process *dp)
{
    p->args = dp->args;
    p->args_len = dp->args_len;
    p->console_size = (shim_client_process_state_console_size *)dp->console_size;
    p->cwd = dp->cwd;
    p->env = dp->env;
    p->env_len = dp->env_len;
    p->terminal = dp->terminal;
    p->user = (shim_client_process_state_user *)dp->user;
    p->capabilities = (shim_client_process_state_capabilities *)dp->capabilities;
    p->apparmor_profile = dp->apparmor_profile;
    p->oom_score_adj = dp->oom_score_adj;
    p->selinux_label = dp->selinux_label;
    p->no_new_privileges = dp->no_new_privileges;
    p->rlimits = (shim_client_process_state_rlimits_element **)dp->rlimits;
    p->rlimits_len = dp->rlimits_len;
}

static int file_write_int(const char *fname, int val)
{
    char sint[UINT_LEN] = {0};

    if (snprintf(sint, sizeof(sint), "%d", val) < 0) {
        return -1;
    }

    if (util_write_file(fname, sint, strlen(sint)) < 0) {
        return -1;
    }

    return 0;
}

/* val will updated only when success. */
static void file_read_int(const char *fname, int *val)
{
    char *sint = NULL;
    int ival = 0;

    if (!util_file_exists(fname)) {
        free(sint);
        return;
    }

    sint = util_read_text_file(fname);
    if (sint == NULL) {
        return;
    }

    if (!util_safe_int(sint, &ival)) {
        *val = ival;
    }

    free(sint);
}

static char *get_err_message(const char *workdir, const char *file)
{
    char fname[PATH_MAX] = {0};
    FILE *fp = NULL;
    char *pline = NULL;
    size_t length = 0;

    if (snprintf(fname, sizeof(fname), "%s/%s", workdir, file) < 0) {
        ERROR("failed make full path %s/%s", workdir, file);
        return NULL;
    }

    fp = util_fopen(fname, "r");
    if (fp == NULL) {
        return NULL;
    }

    while (getline(&pline, &length, fp) != -1) {
        if (pline == NULL) {
            return NULL;
        }
        if (strings_contains_word(pline, "error")) {
            return pline;
        }
    }

    if (pline != NULL) {
        free(pline);
    }
    return NULL;
}

static void show_shim_runtime_errlog(const char *workdir)
{
    char *msg = NULL;

    msg = get_err_message(workdir, "shim-log.json");
    if (msg != NULL) {
        ERROR("shim-log error %s", msg);
        isulad_set_error_message("shim-log error %s", msg);
        free(msg);
    }
    msg = get_err_message(workdir, "log.json");
    if (msg != NULL) {
        ERROR("runtime-log error %s", msg);
        isulad_set_error_message("runtime-log error %s", msg);
        free(msg);
    }
}

bool rt_isula_detect(const char *runtime)
{
    if (runtime != NULL && (strcasecmp(runtime, "lcr") != 0)) {
        return true;
    }

    return false;
}

static int create_process_json_file(const char *workdir, bool exec,
                                    const char *exit_fifo, const char *control_fifo,
                                    const char *stdin, const char *stdout, const char *stderr,
                                    const char **runtime_args, size_t runtime_args_len, defs_process *process)
{
    shim_client_process_state *p = NULL;
    struct parser_context ctx = {OPT_GEN_SIMPLIFY, 0};
    parser_error perr = NULL;
    char *data = NULL;
    char fname[PATH_MAX] = {0};
    int retcode = 0;

    if (snprintf(fname, sizeof(fname), "%s/process.json", workdir) < 0) {
        ERROR("failed make process.json full path");
        return -1;
    }

    p = util_common_calloc_s(sizeof(shim_client_process_state));
    p->exec = exec;
    p->exit_fifo = (char *)exit_fifo;
    p->control_fifo = (char *)control_fifo;
    p->isulad_stdin = (char *)stdin;
    p->isulad_stdout = (char *)stdout;
    p->isulad_stderr = (char *)stderr;
    p->runtime_args = (char **)runtime_args;
    p->runtime_args_len = runtime_args_len;
    copy_process(p, process);

    data = shim_client_process_state_generate_json(p, &ctx, &perr);
    if (data == NULL) {
        retcode = -1;
        ERROR("failed generate json for process.json error=%s", perr);
        goto out;
    }

    if (util_write_file(fname, data, strlen(data)) != 0) {
        retcode = -1;
        ERROR("failed write process.json");
        goto out;
    }

out:
    UTIL_FREE_AND_SET_NULL(perr);
    UTIL_FREE_AND_SET_NULL(p); /* field in p shall free by caller */
    UTIL_FREE_AND_SET_NULL(data);

    return retcode;
}

static void get_runtime_cmd(const char *runtime, const char **cmd)
{
    struct service_arguments *args = NULL;
    defs_map_string_object_runtimes *runtimes = NULL;
    size_t i = 0;

    if (isulad_server_conf_rdlock()) {
        ERROR("failed to lock server config");
        goto out;
    }

    args = conf_get_server_conf();
    if (args == NULL) {
        ERROR("failed to get server config");
        goto unlock_out;
    }

    if (args->json_confs != NULL) {
        runtimes = args->json_confs->runtimes;
    }
    if (runtimes == NULL) {
        goto unlock_out;
    }

    for (i = 0; i < runtimes->len; i++) {
        if (strcmp(runtime, runtimes->keys[i]) == 0) {
            *cmd = runtimes->values[i]->path;
            goto unlock_out;
        }
    }

unlock_out:
    if (isulad_server_conf_unlock()) {
        ERROR("failed to unlock server config");
    }
out:
    if (strcmp(runtime, "runc") == 0) {
        *cmd = "runc";
        return;
    }

    if (strcmp(runtime, "kata-runtime") == 0) {
        *cmd = "kata-runtime";
        return;
    }
    if (*cmd == NULL) {
        ERROR("missing match runtime config for %s", runtime);
    }
}

static int get_runtime_args(const char *runtime, const char ***args)
{
    int ret = 0;
    struct service_arguments *gargs = NULL;
    defs_map_string_object_runtimes *runtimes = NULL;
    size_t i = 0;

    if (isulad_server_conf_rdlock()) {
        ERROR("failed to lock server config");
        goto out;
    }

    gargs = conf_get_server_conf();
    if (gargs == NULL) {
        ERROR("failed to get server config");
        goto unlock_out;
    }

    if (gargs->json_confs != NULL) {
        runtimes = gargs->json_confs->runtimes;
    }
    if (runtimes == NULL) {
        goto unlock_out;
    }

    for (i = 0; i < runtimes->len; i++) {
        if (strcmp(runtime, runtimes->keys[i]) == 0) {
            *args = (const char **)runtimes->values[i]->runtime_args;
            ret = runtimes->values[i]->runtime_args_len;
            goto unlock_out;
        }
    }
unlock_out:
    if (isulad_server_conf_unlock()) {
        ERROR("failed to unlock server config");
    }
out:
    return ret;
}

static bool shim_alive(const char *workdir)
{
    int pid = 0;
    char *fpid = util_path_join(workdir, "shim-pid");
    int ret = 0;

    if (fpid == NULL) {
        ERROR("failed make shim-pid full path");
        return false;
    }

    file_read_int(fpid, &pid);

    if (pid == 0) {
        ERROR("failed read shim-pid file %s", fpid);
        return false;
    }

    ret = kill(pid, 0);
    if (ret != 0) {
        INFO("kill 0 shim-pid with error: %s", strerror(errno));
    }
    return ret == 0;
}

typedef struct {
    const char *workdir;
    const char *runtime;
    const char *cmd;
    const char **args;
    size_t args_len;
    const char *subcmd;
    const char **opts;
    size_t opts_len;
    const char *id;
    char **params;
    size_t params_num;
} runtime_exec_info;

static void runtime_exec_param_dump(const char **params)
{
    char *full = NULL;
    int i = 0;

    for (i = 0; i < PARAM_NUM; i++) {
        if (*(params + i) == NULL) {
            full = util_string_join(" ", params, i);
            INFO("runtime call params[%d] %s", i, full);
            UTIL_FREE_AND_SET_NULL(full);
            return;
        }
    }
}

static void runtime_exec_param_init(char **params, size_t params_num, runtime_exec_info *rei)
{
    size_t i = 0, j = 0;

    add_array_elem(params, params_num, &i, rei->cmd);
    for (j = 0; j < rei->args_len; j++) {
        add_array_elem(params, params_num, &i, *(rei->args + j));
    }

    add_array_elem(params, params_num, &i, rei->subcmd);
    for (j = 0; j < rei->opts_len; j++) {
        add_array_elem(params, params_num, &i, *(rei->opts + j));
    }

    if (rei->id) {
        add_array_elem(params, params_num, &i, rei->id);
    }
}

static void runtime_exec_info_init(runtime_exec_info *rei,
                                   const char *workdir, const char *runtime,
                                   const char *subcmd, const char **opts, size_t opts_len, const char *id,
                                   char **params, size_t params_num)
{
    rei->workdir = workdir;
    rei->runtime = runtime;
    rei->args_len = get_runtime_args(runtime, &rei->args);
    get_runtime_cmd(runtime, &rei->cmd);
    rei->subcmd = subcmd;
    rei->opts = opts;
    rei->opts_len = opts_len;
    rei->id = id;
    rei->params = params;
    rei->params_num = params_num;

    runtime_exec_param_init(rei->params, rei->params_num, rei);
    runtime_exec_param_dump((const char **)rei->params);
}


static void runtime_exec_func(void *arg)
{
    runtime_exec_info *rei = (runtime_exec_info *) arg;

    if (rei == NULL) {
        ERROR("missing runtime exec info");
        return;
    }

    if (util_check_inherited(true, -1) != 0) {
        ERROR("close inherited fds failed");
        return;
    }

    if (chdir(rei->workdir) < 0) {
        ERROR("chdir %s failed", rei->workdir);
        return;
    }

    execvp(rei->cmd, rei->params);
    ERROR("exec %s %s %s failed", rei->cmd, rei->subcmd, rei->id);
}

static int status_string_to_int(const char *status)
{
    if (strcmp(status, "running") == 0) {
        return ENGINE_CONTAINER_STATUS_RUNNING;
    }
    if (strcmp(status, "stopped") == 0) {
        return ENGINE_CONTAINER_STATUS_STOPPED;
    }
    if (strcmp(status, "paused") == 0) {
        return ENGINE_CONTAINER_STATUS_PAUSED;
    }
    return ENGINE_CONTAINER_STATUS_UNKNOWN;
}

static int runtime_call_status(const char *workdir, const char *runtime,
                               const char *id, struct engine_container_status_info *ecsi)
{
    char *stdout = NULL;
    char *stderr = NULL;
    oci_runtime_state *state = NULL;
    struct parser_context ctx = {OPT_GEN_SIMPLIFY, 0};
    parser_error perr = NULL;
    runtime_exec_info rei = { 0 };
    int ret = 0;
    char *params[PARAM_NUM] = {0};

    runtime_exec_info_init(&rei, workdir, runtime, "state", NULL, 0, id, params, PARAM_NUM);

    if (!util_exec_cmd(runtime_exec_func, &rei, NULL, &stdout, &stderr)) {
        ERROR("call runtime status failed: %s", stderr);
        ret = -1;
        goto out;
    }

    if (stdout == NULL) {
        ERROR("call runtime status no stdout");
        ret = -1;
        goto out;
    }

    state = oci_runtime_state_parse_data(stdout, &ctx, &perr);
    if (state == NULL) {
        ERROR("call runtime status parse json failed");
        ret = -1;
        goto out;
    }

    ecsi->status = status_string_to_int(state->status);
    ecsi->pid = state->pid;
    if (state->pid != 0) {
        ecsi->has_pid = true;
    }

    INFO("container %s status %s pid %d", id, state->status, state->pid);

out:
    free_oci_runtime_state(state);
    UTIL_FREE_AND_SET_NULL(stdout);
    UTIL_FREE_AND_SET_NULL(stderr);
    UTIL_FREE_AND_SET_NULL(perr);
    return ret;
}

static int runtime_call_simple(const char *workdir, const char *runtime,
                               const char *subcmd, const char **opts, size_t opts_len, const char *id)
{
    runtime_exec_info rei = {0};
    char *stdout = NULL;
    char *stderr = NULL;
    int ret = 0;
    char *params[PARAM_NUM] = {0};

    runtime_exec_info_init(&rei, workdir, runtime, subcmd, opts, opts_len, id, params, PARAM_NUM);
    if (!util_exec_cmd(runtime_exec_func, &rei, NULL, &stdout, &stderr)) {
        ERROR("call runtime %s failed stderr %s", subcmd, stderr);
        goto out;
    }

out:
    UTIL_FREE_AND_SET_NULL(stdout);
    UTIL_FREE_AND_SET_NULL(stderr);
    return ret;
}

static int runtime_call_delete_force(const char *workdir, const char *runtime, const char *id)
{
    const char *opts[1] = {"-f"};
    return runtime_call_simple(workdir, runtime, "delete", opts, 1, id);
}

#define ExitSignalOffset 128
static int status_to_exit_code(int status)
{
    int exit_code = 0;

    if (WIFEXITED(status)) {
        exit_code = WEXITSTATUS(status);
    } else {
        exit_code = -1;
        exit_code = -1;
    }
    if (WIFSIGNALED(status)) {
        int signal;
        signal = WTERMSIG(status);
        exit_code = ExitSignalOffset + signal;
    }
    return exit_code;
}

static int shim_create(bool fg, const char *id, const char *workdir,
                       const char *bundle, const char *runtime_cmd,
                       int *exit_code)
{
    pid_t pid = 0;
    int exec_fd[2] = { -1, -1 };
    int num = 0;
    int ret = 0;
    char exec_buff[BUFSIZ + 1] = { 0 };
    char *fpid = util_path_join(workdir, "shim-pid");
    const char *params[PARAM_NUM] = {0};
    int i = 0;
    int status = 0;

    params[i++] = SHIM_BINARY;
    params[i++] = id;
    params[i++] = bundle;
    params[i++] = runtime_cmd;
    params[i++] = "info";
    params[i++] = "2m0s";
    runtime_exec_param_dump(params);

    if (fpid == NULL) {
        ERROR("failed make shim-pid full path");
        return -1;
    }

    if (pipe2(exec_fd, O_CLOEXEC) != 0) {
        ERROR("failed to create pipe for shim create");
        free(fpid);
        return -1;
    }

    pid = fork();
    if (pid < 0) {
        ERROR("failed fork for shim parent %s", strerror(errno));
        close(exec_fd[0]);
        close(exec_fd[1]);
        free(fpid);
        return -1;
    }

    if (pid == (pid_t)0) {
        if (chdir(workdir) < 0) {
            (void)dprintf(exec_fd[1], "%s: failed chdir to %s", id, workdir);
            exit(EXIT_FAILURE);
        }

        if (fg) {
            goto realexec;
        }

        pid = fork();
        if (pid < 0) {
            (void)dprintf(exec_fd[1], "%s: fork shim-process failed %s", id, strerror(errno));
            _exit(EXIT_FAILURE);
        }
        if (pid != 0) {
            if (file_write_int(fpid, pid) != 0) {
                (void)dprintf(exec_fd[1], "%s: write %s with %d failed", id, fpid, pid);
            }
            _exit(EXIT_SUCCESS);
        }

realexec:
        /* real shim process. */
        close(exec_fd[0]);
        if (setsid() < 0) {
            (void)dprintf(exec_fd[1], "%s: failed setsid for process %d", id, getpid());
            exit(EXIT_FAILURE);
        }

        if (util_check_inherited(true, exec_fd[1]) != 0) {
            (void)dprintf(exec_fd[1], "close inherited fds failed");
        }

        execvp(SHIM_BINARY, (char * const *)params);
        (void)dprintf(exec_fd[1], "exec failed: %s", strerror(errno));
    }

    close(exec_fd[1]);
    num = util_read_nointr(exec_fd[0], exec_buff, sizeof(exec_buff));
    close(exec_fd[0]);
    if (num > 0) {
        ERROR("exec failed: %s", exec_buff);
        ret = -1;
        goto out;
    }

    status = wait_for_pid_status(pid);
    if (status < 0) {
        ERROR("failed wait shim-parent %d exit %s", pid, strerror(errno));
        ret = -1;
        goto out;
    }

    if (exit_code != NULL) {
        *exit_code = status_to_exit_code(status);
    }

out:
    if (ret != 0) {
        show_shim_runtime_errlog(workdir);
        kill(pid, SIGKILL);             /* can kill other process? */
    }
    free(fpid);

    return ret;
}


static int get_container_process_pid(const char *workdir)
{
    char fname[PATH_MAX] = {0};
    int pid = 0;
    struct timespec beg = {0};
    struct timespec end = {0};

    if (snprintf(fname, sizeof(fname), "%s/pid", workdir) < 0) {
        ERROR("failed make pid full path");
        return -1;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &beg) != 0) {
        ERROR("failed get time");
        return -1;
    }

    while (1) {
        if (clock_gettime(CLOCK_MONOTONIC, &end) != 0) {
            ERROR("failed get time");
            return -1;
        }
        if (end.tv_sec - beg.tv_sec > PID_WAIT_TIME) {
            ERROR("wait container process pid timeout %s", workdir);
            return -1;
        }
        file_read_int(fname, &pid);
        if (!pid) {
            if (shim_alive(workdir)) {
                usleep_nointerupt(100000);
                continue;
            }
            ERROR("failed read pid from dead shim %s", workdir);
            return -1;
        }
        return pid;                     /* success */
    }
    return -1;
}

static void shim_kill_force(const char *workdir)
{
    int pid = 0;
    char *fpid = util_path_join(workdir, "shim-pid");

    if (fpid == NULL) {
        INFO("shim-pid not exist");
        return;
    }

    file_read_int(fpid, &pid);

    if (pid == 0) {
        goto out;
    }

    kill(pid, SIGKILL);

out:
    free(fpid);
    INFO("kill shim force %s", workdir);
}

int rt_isula_create(const char *id, const char *runtime,
                    const rt_create_params_t *params)
{
    oci_runtime_spec *config = params->oci_config_data;
    const char *cmd = NULL;
    const char **runtime_args = NULL;
    size_t runtime_args_len = get_runtime_args(runtime, &runtime_args);
    int ret = 0;
    char *workdir = util_path_join(params->state, id);

    ret = create_process_json_file(workdir, false, params->exit_fifo, NULL,
                                   params->stdin, params->stdout, params->stderr, runtime_args,
                                   runtime_args_len, config->process);
    if (ret != 0) {
        ERROR("%s: failed create json file", id);
        goto out;
    }

    get_runtime_cmd(runtime, &cmd);
    ret = shim_create(false, id, workdir, params->bundle, cmd, NULL);
    if (ret != 0) {
        runtime_call_delete_force(workdir, runtime, id);
        ERROR("%s: failed create shim process", id);
        goto out;
    }

out:
    free(workdir);
    return ret;
}

int rt_isula_start(const char *id, const char *runtime,
                   const rt_start_params_t *params,
                   container_pid_t *pid_info)
{
    char *workdir = util_path_join(params->state, id);
    pid_t pid = 0;
    int ret = 0;

    if (workdir == NULL) {
        ERROR("%s: missing shim workdir", id);
        return -1;
    }

    pid = get_container_process_pid(workdir);
    if (pid < 0) {
        ret = -1;
        ERROR("%s: failed wait init pid", id);
        goto out;
    }

    if (container_read_proc(pid, pid_info) != 0) {
        ret = -1;
        ERROR("%s: failed read pid info", id);
        goto out;
    }

    if (runtime_call_simple(workdir, runtime, "start", NULL, 0, id) != 0) {
        ERROR("call runtime start id failed");
        ret = -1;
        goto out;
    }

out:
    if (ret != 0) {
        show_shim_runtime_errlog(workdir);
        shim_kill_force(workdir);
    }
    free(workdir);

    return ret;
}

int rt_isula_restart(const char *name, const char *runtime,
                     const rt_restart_params_t *params)
{
    ERROR(">>> restart not implemented");
    return RUNTIME_NOT_IMPLEMENT_RESET;
}

int rt_isula_clean_resource(const char *id, const char *runtime,
                            const rt_clean_params_t *params)
{
    char workdir[PATH_MAX] = {0};

    if (snprintf(workdir, sizeof(workdir), "%s/%s", params->statepath, id) < 0) {
        ERROR("failed get shim workdir");
        return -1;
    }

    if (shim_alive(workdir)) {
        shim_kill_force(workdir);
    }

    (void)runtime_call_delete_force(workdir, runtime, id);

    if (util_recursive_rmdir(workdir, 0) != 0) {
        ERROR("failed rmdir -r shim workdir");
        return -1;
    }

    INFO("rmdir -r %s done", workdir);
    return 0;
}

int rt_isula_rm(const char *id, const char *runtime, const rt_rm_params_t *params)
{
    char libdir[PATH_MAX] = {0};

    if (snprintf(libdir, sizeof(libdir), "%s/%s", params->rootpath, id) < 0) {
        ERROR("failed get shim workdir");
        return -1;
    }

    if (util_recursive_rmdir(libdir, 0) != 0) {
        ERROR("failed rmdir -r shim workdir");
        return -1;
    }

    INFO("rmdir -r %s done", libdir);
    return 0;
}

static char *try_generate_exec_id()
{
    char *id = NULL;

    id = util_common_calloc_s(sizeof(char) * (CONTAINER_EXEC_ID_MAX_LEN + 1));
    if (id == NULL) {
        ERROR("Out of memory");
        return NULL;
    }

    if (util_generate_random_str(id, (size_t)CONTAINER_EXEC_ID_MAX_LEN)) {
        ERROR("Generate id failed");
        goto err_out;
    }

    return id;

err_out:
    free(id);
    return NULL;
}

static bool fg_exec(const rt_exec_params_t *params)
{
    if (params->console_fifos[0] != NULL || params->console_fifos[1] != NULL ||
        params->console_fifos[2] != NULL) {
        return true;
    }
    return false;
}

int rt_isula_exec(const char *id, const char *runtime,
                  const rt_exec_params_t *params, int *exit_code)
{
    char *exec_id = NULL;
    defs_process *process = params->spec;
    const char **runtime_args = NULL;
    size_t runtime_args_len = get_runtime_args(runtime, &runtime_args);
    char workdir[PATH_MAX] = {0};
    const char *cmd = NULL;
    int ret = 0;
    char bundle[PATH_MAX] = {0};
    int pid = 0;

    ret = snprintf(bundle, sizeof(bundle), "%s/%s", params->rootpath, id);
    if (ret < 0) {
        ERROR("failed join bundle path for exec");
        goto out;
    }

    exec_id = try_generate_exec_id();
    if (exec_id == NULL) {
        ERROR("Failed to generate exec id");
        ret = -1;
        goto out;
    }

    ret = snprintf(workdir, sizeof(workdir), "%s/%s/exec/%s", params->state, id, exec_id);
    if (ret < 0) {
        ERROR("failed join exec full path");
        goto out;
    }
    ret = util_mkdir_p(workdir, DEFAULT_SECURE_DIRECTORY_MODE);
    if (ret < 0) {
        ERROR("failed mkdir exec workdir %s", workdir);
        goto out;
    }

    ret = create_process_json_file(workdir, true, NULL, NULL,
                                   params->console_fifos[0], params->console_fifos[1], params->console_fifos[2],
                                   runtime_args, runtime_args_len, process);
    if (ret != 0) {
        ERROR("%s: failed create exec json file");
        goto out;
    }

    get_runtime_cmd(runtime, &cmd);
    ret = shim_create(fg_exec(params), id, workdir, bundle, cmd, exit_code);
    if (ret != 0) {
        ERROR("%s: failed create shim process for exec %s", id, exec_id);
        goto out;
    }

    pid = get_container_process_pid(workdir);
    if (pid < 0) {
        ERROR("%s: failed get exec process id", workdir);
        ret = -1;
        goto out;
    }

out:
    if (ret != 0) {
        show_shim_runtime_errlog(workdir);
    } else {
        if (util_recursive_rmdir(workdir, 0)) {
            ERROR("rmdir %s failed", workdir);
        }
    }
    return ret;
}

int rt_isula_status(const char *id, const char *runtime,
                    const rt_status_params_t *params,
                    struct engine_container_status_info *status)
{
    char workdir[PATH_MAX] = {0};
    int ret = 0;

    ret = snprintf(workdir, sizeof(workdir), "%s/%s", params->state, id);
    if (ret < 0) {
        ERROR("failed join full workdir %s/%s", params->rootpath, id);
        goto out;
    }

    if (!shim_alive(workdir)) {
        ERROR("shim dead %s", workdir);
        ret = -1;
        goto out;
    }

    ret = runtime_call_status(workdir, runtime, id, status);

out:
    return ret;
}

int rt_isula_attach(const char *id, const char *runtime,
                    const rt_attach_params_t *params)
{
    ERROR("rt_isula_attach not impl");
    return 0;
}

int rt_isula_update(const char *id, const char *runtime, const rt_update_params_t *params)
{
    ERROR("rt_isula_update not impl");
    return 0;
}

int rt_isula_pause(const char *id, const char *runtime, const rt_pause_params_t *params)
{
    char workdir[PATH_MAX] = {0};

    if (snprintf(workdir, sizeof(workdir), "%s/%s", params->state, id) < 0) {
        ERROR("failed join workdir %s/%s", params->state, id);
        return -1;
    }

    return runtime_call_simple(workdir, runtime, "pause", NULL, 0, id);
}

int rt_isula_resume(const char *id, const char *runtime, const rt_resume_params_t *params)
{
    char workdir[PATH_MAX] = {0};

    if (snprintf(workdir, sizeof(workdir), "%s/%s", params->state, id) < 0) {
        ERROR("failed join workdir %s/%s", params->state, id);
        return -1;
    }

    return runtime_call_simple(workdir, runtime, "resume", NULL, 0, id);
}

int rt_isula_listpids(const char *name, const char *runtime, const rt_listpids_params_t *params, rt_listpids_out_t *out)
{
    ERROR("rt_isula_listpids not impl");
    return 0;
}

int rt_isula_resources_stats(const char *name, const char *runtime,
                             const rt_stats_params_t *params,
                             struct engine_container_resources_stats_info *rs_stats)
{
    ERROR("rt_isula_resources_stats not impl");
    return 0;
}

int rt_isula_resize(const char *id, const char *runtime, const rt_resize_params_t *params)
{
    ERROR("rt_isula_resize not impl");
    return 0;
}

int rt_isula_exec_resize(const char *id, const char *runtime, const rt_exec_resize_params_t *params)
{
    ERROR("rt_isula_exec_resize not impl");
    return 0;
}
