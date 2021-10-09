#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include "isula_libutils/log.h"
#include "volume_api.h"
#include "utils.h"
#include "map.h"
#include "local.h"
#include "err_msg.h"
#include "utils_file.h"
#include <lxc/lxccontainer.h>
#include "container_api.h"
#include "image_api.h"
#include "checkpoint_api.h"

/*
 * 根据容器Id，checkpoint Id，以及checkpoint路径去创建一个checkpoint
 */

//判断是否为目录
bool is_dir(const char *path)
{
    struct stat statbuf;
    if(lstat(path, &statbuf) ==0)//lstat返回文件的信息，文件信息存放在stat结构中
    {
        return S_ISDIR(statbuf.st_mode) != 0;//S_ISDIR宏，判断文件类型是否为目录
    }
    return false;
}

//判断是否为常规文件
bool is_file(const char *path)
{
    struct stat statbuf;
    if(lstat(path, &statbuf) ==0)
        return S_ISREG(statbuf.st_mode) != 0;//判断文件是否为常规文件
    return false;
}

//判断是否是特殊目录
bool is_special_dir(const char *path)
{
    return strcmp(path, ".") == 0 || strcmp(path, "..") == 0;
}

//生成完整的文件路径
void get_file_path(const char *path, const char *file_name,  char *file_path)
{
    strcpy(file_path, path);
    if(file_path[strlen(path) - 1] != '/')
        strcat(file_path, "/");
    strcat(file_path, file_name);
}

int delete_file(const char *path)
{
    printf(" delete_file\n");
    DIR *dir;
    struct dirent *dir_info;
    char file_path[1000];
    
    if(is_file(path))
    {
        remove(path);
        return 0;
    }else if(is_dir(path))
    {
        if((dir = opendir(path)) == NULL)
            return -1;
      
        while((dir_info = readdir(dir)) != NULL)
        {
            get_file_path(path, dir_info->d_name, file_path);
            if(is_special_dir(dir_info->d_name))
                continue;
            delete_file(file_path);
            rmdir(file_path);
        }
        rmdir(path);
    }else{
        return -1;
    }
    return 0;
}
int checkpoint_remove(char* container,char* dir)
{

    if(!dir){
        dir="/tmp/isula-criu/";
    }
    char checkpoint_path[1000]={0};
    strcat(checkpoint_path,dir);
    strcat(checkpoint_path,container);

    if(delete_file(checkpoint_path)!=0){
        printf("sdelete fail\n");
        isulad_set_error_message("Checkpointing remove %s failed",container);
        return -1;
    }
    printf("delete success\n");
    return 0;
}



int checkpoint_create(char* container,char* dir)
{
    
    struct lxc_container *c;
    c=lxc_container_new(container,"/var/lib/isulad/engines/lcr/");
    //无法获取容器
    if (!c) {
		isulad_set_error_message("System error loading %s\n", container);
		return -1;
	}
    //容器未定义
    if (!c->is_defined(c)) {
		isulad_set_error_message("(deamon modules)Error response from daemon: No such container:%s\n",container);
		return -1;
	}
    //容器未运行
    if (!c->is_running(c)) {
		isulad_set_error_message("%s not running, not checkpointing\n", container);
		return -1;
	}

    //如果自定义了路径就使用自定义路径，否则使用默认路径
    if(!dir){
        dir="/tmp/isula-criu/";
    }

    char checkpoint_path[1000]={0};
    strcat(checkpoint_path,dir);
    strcat(checkpoint_path,container);
    bool res;
printf("checkpoint_path:%s\n",checkpoint_path);
    res =  c->checkpoint(c,checkpoint_path,true,false);
    
    if(!res){
        printf("checkpoint_path:%s\n",checkpoint_path);
        isulad_set_error_message("Checkpointing %s failed",container);
        return -1;
    }
    return 0;
}

int checkpoint_restore(char* container,char* dir)
{
    container_t *cont=NULL;
    cont = containers_store_get(container);
    im_mount_container_rootfs(cont->common_config->image_type, cont->common_config->image, container);
    struct lxc_container *c;
    c=lxc_container_new(container,"/var/lib/isulad/engines/lcr/");
    if (!c) {
		isulad_set_error_message("System error loading %s\n", container);
		return -1;
	}
    if (!c->is_defined(c)) {
		isulad_set_error_message("Error response from daemon: No such container:%s\n", container);
		return -1;
	}
    if (c->is_running(c)) {
		isulad_set_error_message("%s is running, not restoring\n", container);
		return -1;
	}

     if(!dir){
        dir="/tmp/isula-criu/";
    }

    char checkpoint_path[1000]={0};
    strcat(checkpoint_path,dir);
    strcat(checkpoint_path,container);
    bool res;

    res =  c->restore(c,checkpoint_path,true);

    
    if (!res){
        isulad_set_error_message("Restoring %s failed\n",container);
        return -1;
    }

    pid_ppid_info_t pid_info = { 0 };
    container_state_set_running(cont->state, &pid_info, true);
    container_state_reset_has_been_manual_stopped(cont->state);
    container_init_health_monitor(cont->common_config->id);
    if (container_state_to_disk(cont)) {
        isulad_set_error_message("Failed to save container \"%s\" to disk", cont->common_config->id);
        return -1;
    }
    
    return 0;
}

static struct checkpoints *new_empty_checkpoints(int size)
{
    struct checkpoints *ches = NULL;

    ches = util_common_calloc_s(sizeof(struct checkpoints));
    if (ches == NULL) {
        ERROR("out of memory");
        return NULL;
    }

    if (size == 0) {
        return ches;
    }

    ches->ches = util_common_calloc_s(sizeof(struct checkpoint*) * size);
    if (ches->ches == NULL) {
        ERROR("out of memory");
        free(ches);
        return NULL;
    }

    return ches;
}

static struct checkpoint * dup_checkpoint(char *name, char *dir)
{
    struct checkpoint *che = NULL;

    che = util_common_calloc_s(sizeof(struct checkpoint));
    if (che == NULL) {
        ERROR("out of memory");
        return NULL;
    }

    che->dir = util_strdup_s(dir);
    che->name = util_strdup_s(name);
    return che;
}


struct checkpoints * checkpoint_list(char* dir){
    

    struct checkpoints *ches = NULL;
    DIR *dirp;
    struct dirent *dp;
    if(!dir){
        dir="/tmp/isula-criu/";
    }
    dirp=opendir(dir);
    if(dirp==NULL){
        isulad_set_error_message("checkpoint_dir is NULL");
        return NULL;
    }
    
    
    int sum=0;
    while((dp=readdir(dirp))!=NULL){
        if(strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0){
            continue;
        }
        if(containers_store_get(dp->d_name)==NULL){
            continue;
        }
        sum++;
        printf("%s\n",dp->d_name);
    }
    

     ches = new_empty_checkpoints(sum);

      if (ches == NULL) {
        ERROR("out of memory");
        return NULL;
    }
   
  
    dirp=opendir(dir);
   
    while((dp=readdir(dirp))!=NULL){
        if(strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0){
            continue;
        }
         if(containers_store_get(dp->d_name)==NULL){
            continue;
        }
        sum++;
        
        struct checkpoint *che = dup_checkpoint(dp->d_name,dir);
        if (che == NULL) {
            ERROR("out of memory");
            goto out;
        }
         ches->ches[ches->ches_len] = che;
        ches->ches_len++;
    }
   
    
out:
    (void)closedir(dirp);
    return ches;
}



