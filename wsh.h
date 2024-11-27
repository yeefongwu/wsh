#ifndef WSH_H
#define WSH_H

#include <sys/types.h>
#include <string.h>

typedef struct {
    int id; //-1 mean not exist
    pid_t pid;
    char* command;  // The entire command string
} Job;


void fg_command(int job_id);
void bg_command();
int get_next_job();
int get_biggest_job();
void sigchld_handler(int signo);
void add_job(pid_t pid, char *command);
void remove_job(pid_t pid);
void list_jobs() ;
void execute_piped_commands(char **args, int argc,char* command);
void execute_command(char **args,int argc,char* command);

#endif /* WSH_H */
