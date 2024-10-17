#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>

#define MAXJOBS 32
#define MAXJOBID 2147483647
#define MAXLINE 1024

char **environ;
int num_jobs;
int active_jobs;
typedef enum{
    empty,
    finished,
    running,
    suspended
}Status;
typedef struct{
int process_number;
int process_id;
const char* process_name;
Status process_status;
}job;

job jobs_list[32];

// TODO: you will need some data structure(s) to keep track of jobs

void handle_sigchld(int sig) {
    // TODO
}

void handle_sigtstp(int sig) {
    // TODO
}

void handle_sigint(int sig) {
    // TODO
}

void handle_sigquit(int sig) {
    // TODO
}

void install_signal_handlers() {
    // TODO
}

void spawn(const char **toks, bool bg) { // bg is true iff command ended with &
  if (active_jobs > MAXJOBS){
    fprintf(stderr, "ERROR: too many jobs");
  }  
  if(bg){	
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);		
    pid_t new_pid = fork();
    if(new_pid == 0){
      sigprocmask(SIG_UNBLOCK, &mask, NULL);
      const char* command = toks[0]; 
      int error = execvp(command, (char* const*)toks);
      if (error < 0){
        fprintf(stderr, "ERROR: cannot run %s \n", command);
      }
      return;
    }else if(new_pid == -1){
      fprintf(stderr, "ERROR: cannot run %s \n", toks[0]);
    }else{
      job new_job;
      num_jobs++;
      active_jobs++;
      new_job.process_number = num_jobs;
      new_job.process_id = new_pid;
      new_job.process_name = strdup(toks[0]);
      new_job.process_status = running;
      if(num_jobs > MAXJOBS){
      	job current_job;
	for(int job = 0; job < MAXJOBS; job++){
	  current_job = jobs_list[job];
	  if(current_job.process_status == empty){
	    jobs_list[job] = new_job;
       	    break;
	  }
	} 
      }else{
	int index = num_jobs - 1;
        jobs_list[index] = new_job;
      }
	printf("[%d]  %d  running  %s \n", num_jobs, new_pid, new_job.process_name);
      	sigprocmask(SIG_UNBLOCK, &mask, NULL);
	return;
    }
  }else{} //not bg
}
void cmd_jobs(const char **toks) {
   if(toks[1] != NULL){
     fprintf(stderr, "ERROR: jobs takes no arguments\n");
   }else{
     job current_job;   
     bool print;
     char* status;
     for(int index = 0; index < MAXJOBS; index++){
       current_job = jobs_list[index];
       switch(current_job.process_status){
         case running: 
	   print = true;
	   status = "running";
	   break;
	 case suspended: 
	   print = true;
	   status = "running";
	   break;
	 default: 
	   print = false;
	   break;	   
       }
       if(print){
	 const char* name = "ls";
  	 printf("[%d]  %d  %s  %s \n", current_job.process_number, current_job.process_id, status, current_job.process_name);
       }
     }
   }
}

void cmd_fg(const char **toks) {
    // TODO
}

void cmd_bg(const char **toks) {
    // TODO
}

void cmd_nuke(const char **toks) {
    // TODO
}

void cmd_quit(const char **toks) {
    if (toks[1] != NULL) {
        fprintf(stderr, "ERROR: quit takes no arguments\n");
    } else {
        exit(0);
    }
}

void eval(const char **toks, bool bg) { // bg is true iff command ended with &
    assert(toks);
    if (*toks == NULL) return;
    if (strcmp(toks[0], "quit") == 0) {
        cmd_quit(toks);
    } else if(strcmp(toks[0], "jobs") == 0) {
   	cmd_jobs(toks); 
    } else if(strcmp(toks[0], "nuke")==0) {
     	cmd_nuke(toks);
    } else if(strcmp(toks[0], "fg")==0) {
	cmd_fg(toks);
    } else if((strcmp(toks[0], "bg")==0)){
	cmd_bg(toks);
    } else{  
        spawn(toks, bg);
    }
    return;
}

// you don't need to touch this unless you want to add debugging
void parse_and_eval(char *s) {
    assert(s);
    const char *toks[MAXLINE + 1];
    
    while (*s != '\0') {
        bool end = false;
        bool bg = false;
        int t = 0;

        while (*s != '\0' && !end) {
            while (*s == '\n' || *s == '\t' || *s == ' ') ++s;
            if (*s != ';' && *s != '&' && *s != '\0') toks[t++] = s;
            while (strchr("&;\n\t ", *s) == NULL) ++s;
            switch (*s) {
            case '&':
                bg = true;
                end = true;
                break;
            case ';':
                end = true;
                break;
            }
            if (*s) *s++ = '\0';
        }
        toks[t] = NULL;
        eval(toks, bg);
    }
}

// you don't need to touch this unless you want to add debugging
void prompt() {
    printf("crash> ");
    fflush(stdout);
}

// you don't need to touch this unless you want to add debugging
int repl() {
    char *buf = NULL;
    size_t len = 0;
    while (prompt(), getline(&buf, &len, stdin) != -1) {
        parse_and_eval(buf);
    }

    if (buf != NULL) free(buf);
    if (ferror(stdin)) {
        perror("ERROR");
        return 1;
    }
    return 0;
}

// you don't need to touch this unless you want to add debugging options
int main(int argc, char **argv) {
    install_signal_handlers();
    return repl();
}
