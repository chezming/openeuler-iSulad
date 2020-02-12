#ifndef ISULA_RT_OPS_H
#define ISULA_RT_OPS_H /* ISULA_RT_OPS_H */

#include "runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

bool rt_isula_detect(const char *runtime);
int rt_isula_create(const char *name, const char *runtime, const rt_create_params_t *params);
int rt_isula_start(const char *name, const char *runtime, const rt_start_params_t *params, container_pid_t *pid_info);
int rt_isula_restart(const char *name, const char *runtime, const rt_restart_params_t *params);
int rt_isula_clean_resource(const char *name, const char *runtime, const rt_clean_params_t *params);
int rt_isula_rm(const char *name, const char *runtime, const rt_rm_params_t *params);
int rt_isula_exec(const char *id, const char *runtime, const rt_exec_params_t *params, int *exit_code);


int rt_isula_status(const char *name, const char *runtime, const rt_status_params_t *params,
                   struct engine_container_status_info *status);
int rt_isula_attach(const char *id, const char *runtime, const rt_attach_params_t *params);
int rt_isula_update(const char *id, const char *runtime, const rt_update_params_t *params);
int rt_isula_pause(const char *id, const char *runtime, const rt_pause_params_t *params);
int rt_isula_resume(const char *id, const char *runtime, const rt_resume_params_t *params);
int rt_isula_listpids(const char *name, const char *runtime, const rt_listpids_params_t *params, rt_listpids_out_t *out);
int rt_isula_resources_stats(const char *name, const char *runtime, const rt_stats_params_t *params,
                           struct engine_container_resources_stats_info *rs_stats);
int rt_isula_resize(const char *id, const char *runtime, const rt_resize_params_t *params);
int rt_isula_exec_resize(const char *id, const char *runtime, const rt_exec_resize_params_t *params);


#ifdef __cplusplus
}
#endif

#endif /* ISULA_RT_OPS_H */
