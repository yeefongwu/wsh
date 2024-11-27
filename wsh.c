#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include "wsh.h"

Job jobs[256];//all background job
pid_t last_job_pid;

void fg_command(int job_id) {
    if(kill(-jobs[job_id-1].pid, SIGCONT) < 0) perror("kill");
    Job job = jobs[job_id-1];
    jobs[job_id-1].id=-1;
    int status;
    //give child std input
    tcsetpgrp(STDIN_FILENO, job.pid);  
    // wait for it to finish
    waitpid(job.pid, &status, WUNTRACED);  
    // give std input to shell
    tcsetpgrp(STDIN_FILENO, getpgrp());
    if(WIFSTOPPED(status)) add_job(last_job_pid,job.command);
}

void bg_command() {
    //continue in background
    if(kill(-last_job_pid, SIGCONT) < 0) perror("kill");
}

int get_next_job() {
    int index = 0; 
    for(int i=0;i<256;i++){
        if(jobs[i].id==-1) return index;
        index+=1;
    }
    return index;
}

int get_biggest_job() {
    int index = 255; 
    for(int i=255;i>0;i--){
        if(jobs[i].id!=-1) return index;
        index-=1;
    }
    return index;
}

void sigchld_handler(int signo) {
    pid_t pid;
    int wstatus;
    while ((pid = waitpid(-1, &wstatus, WNOHANG)) > 0) {
        if(WIFEXITED(wstatus)){
            tcsetpgrp(2,getpgid(getpid()));
            remove_job(pid);//if finished remove from jobs
        }
    }
}

void add_job(pid_t pid, char *command) {
    int job_add_id = get_next_job();
    jobs[job_add_id].id = job_add_id+1;
    jobs[job_add_id].pid = pid;
    jobs[job_add_id].command = strdup(command); 
}

void remove_job(pid_t pid) {
    int index = -1;
        for (int i = 0; i < 256; i++) {
        if (jobs[i].pid == pid) {
            index = i;
            break;
        }
    }
    if (index == -1) return;
    jobs[index].id=-1;
}

void list_jobs() {
    for (int i = 0; i < 256; i++) {
        if(jobs[i].id==-1) continue;
        printf("%d: %s\n", jobs[i].id, jobs[i].command);
    }
}

void execute_piped_commands(char **args, int argc,char* command) {
    int is_back=0; //0 for not back, 1 for is back
    if(strcmp(args[argc-1],"&")==0) {
        is_back=1;
        args[argc-1] = NULL;
        argc-=1;
    }
    pid_t pgid = 0;  // Initialize process group ID

    int num_pipes = 0;
    for (int i = 0; i < argc; i++) {
        if (strcmp(args[i], "|") == 0) {
            num_pipes++;
        }
    }

    int pfd[2 * num_pipes];
    for (int i = 0; i < num_pipes; i++) {
        if (pipe(pfd + i*2) < 0) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    int cmd_start_idx = 0;
    int pipe_idx = 0;
    for (int i = 0; i < argc; i++) {
        if (i == argc-1 || strcmp(args[i], "|") == 0) {
            int cmd_length = i - cmd_start_idx;
            if (i == argc-1) {
                cmd_length++;
            }
            char *cmd[cmd_length + 1];
            memcpy(cmd, args + cmd_start_idx, cmd_length * sizeof(char *));
            cmd[cmd_length] = NULL;

            pid_t pid = fork();
            if (pid == 0) {
                signal(SIGINT,SIG_DFL);
                signal(SIGTSTP,SIG_DFL);
                signal(SIGQUIT,SIG_DFL);
                if(pipe_idx != 0){
                    dup2(pfd[(pipe_idx-1)*2], 0);
                }
                if(i != argc-1){
                    dup2(pfd[pipe_idx*2 + 1], 1);
                }
                for (int j = 0; j < 2*num_pipes; j++) {
                    close(pfd[j]);
                }
                if (pgid == 0) pgid = getpid();  // For the first process, set the PGID to its own PID
                setpgid(getpid(), pgid);
                execvp(cmd[0],cmd);
            }else{
                signal(SIGTTOU, SIG_IGN);
                if (pgid == 0) pgid = pid;  // For the first child, set the PGID to its PID
            }

            cmd_start_idx = i + 1;
            pipe_idx++;
        }
    }

    for (int i = 0; i < 2 * num_pipes; i++) {
        close(pfd[i]);
    }
    if(is_back==0) for (int i = 0; i <= num_pipes; i++) wait(NULL);            
    else add_job(pgid,command);
}

void execute_command(char **args,int argc,char* command) {
    pid_t pid;
    if (args[0] == NULL) return;

    if (strcmp(args[0], "exit") == 0) {
        if(argc!=1) exit(-1);
        else exit(0);
    }

    if(strcmp(args[0],"cd")==0){
        if(argc!=2) exit(-1);
        if(chdir(args[1])!=0) exit(-1);
        return;
    }

    if (strcmp(args[0], "jobs") == 0) {
        list_jobs();
        return;
    }

    if(strcmp(args[0],"fg")==0){
        if(argc==1) fg_command(get_biggest_job()+1);
        else fg_command(atoi(args[1]));
        return;
    }
    
    if(strcmp(args[0],"bg")==0){
        bg_command();
        return;
    }

    pid = fork();
    if(pid==0){
        signal(SIGINT,SIG_DFL);
        signal(SIGTSTP,SIG_DFL);
        signal(SIGQUIT,SIG_DFL);
        if(strcmp(args[argc-1],"&")==0)  args[argc-1] = NULL;
        execvp(args[0], args);
    }else{
        signal(SIGTTOU, SIG_IGN);
        if(strcmp(args[argc-1],"&")==0) add_job(pid,command);
        else {
            last_job_pid = pid;
            setpgid(pid, pid);
            tcsetpgrp(STDIN_FILENO, pid);
            int status;
            waitpid(pid,&status,WUNTRACED);
            tcsetpgrp(STDIN_FILENO, getpgrp());
            if(WIFSTOPPED(status)) add_job(pid,command);  
        }
    }
}

int main(int argc, char *argv[]) {
    signal(SIGTSTP, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    signal(SIGCHLD, sigchld_handler);

    for(int i=0;i<256;i++){jobs[i].id=-1;}
    char *line = NULL;
    size_t len = 0;
    FILE *input_source = stdin;

    // If a batch file is provided, open it and switch the mode
    int batch_mode = 0;
    if (argc == 2) {
        input_source = fopen(argv[1], "r");
        batch_mode = 1;
    }

    while (1) {
        if (!batch_mode) printf("wsh> ");
        //for ctrl+d
        if (getline(&line, &len, input_source) == -1) break;

        len = strlen(line);
        if (len > 0 && line[len-1] == '\n')  line[len-1] = '\0';
        char* command = strdup(line);
        // Simple parsing: split by spaces
        char **args = malloc(sizeof(char *) * 64);
        char *token;
        int argc = 0;
        while ((token = strsep(&line, " ")) != NULL) args[argc++] = token;
        args[argc] = NULL;
        if(strchr(command, '|')!=NULL) execute_piped_commands(args,argc,command);
        else execute_command(args,argc,command);
        free(args);
    }

    return 0;
}
