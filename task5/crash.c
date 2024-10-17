#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>
#include <termios.h>

#define MAXJOBS 32
#define MAXJOBID 2147483647
#define MAXLINE 1024

char **environ;
int num_jobs;
int active_jobs;
struct termios crash_termios;
typedef enum{
    empty,
    finished,
    running,
    suspended
}Status;
typedef struct{
int process_number;
pid_t process_id;
const char* process_name;
Status process_status;
bool fg;
char exit_msg[40];
char kill_msg[40];
char suspend_msg[60];
char core_dumpd_msg[80];
}job;

// sprintf(exit_msg,"[%d] (%d)  killed %s", job.process_number, )

job jobs_list[32];


int find_job(pid_t pid){	
    job current_job;
    for (int index = 0; index < MAXJOBS; index++){
      current_job = jobs_list[index];
      if(current_job.process_id == pid){
      	return index;
      }
    }
    return -1;
}

void handle_sigchld(int sig) {
    int status;
    job child;
    int child_index;
    pid_t child_pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED);
    while(child_pid != -1){
      child_index = find_job(child_pid);
      if(child_index!= -1){
      	child = jobs_list[child_index];
	if(WIFEXITED(status)){
	  //child process exited normally
	  if(!child.fg){
	    jobs_list[child_index].process_status = empty;	
	    write(STDOUT_FILENO, child.exit_msg, strlen(child.exit_msg)); 
	  }	
	}else if(WIFSIGNALED(status)){
	  //child process terminated by a signal
	  if(WCOREDUMP(status)){
	    //child process dumped core
	    jobs_list[child_index].process_status = empty;	
	    write(STDOUT_FILENO, child.core_dumpd_msg, strlen(child.core_dumpd_msg));  
	  }else{
	    jobs_list[child_index].process_status = empty;	
	    write(STDOUT_FILENO, child.kill_msg, strlen(child.kill_msg)); 
	  } 
	}else if(WIFSTOPPED(status)){
	  //child stopped by a signal
	  jobs_list[child_index].process_status = suspended;	
	  write(STDOUT_FILENO, child.suspend_msg, strlen(child.kill_msg)); 
	}else if(WIFCONTINUED(status)){
	  //suspended process is now being continued
	  jobs_list[child_index].process_status = running;
	}
      }
      child_pid = waitpid(-1, &status, WNOHANG);
    }
}

void handle_sigtstp(int sig) {
  job current_job;
  for(int index = 0; index < MAXJOBS; index++){
    current_job = jobs_list[index];
    if(current_job.fg){
      if(current_job.process_status == running){
        kill(current_job.process_id, SIGSTOP);
      }
    }
  }
}


void handle_sigint(int sig) {
  job current_job;
  for(int index = 0; index < MAXJOBS; index++){
    current_job = jobs_list[index];
    if(current_job.fg){
      if(current_job.process_status == running){
        kill(current_job.process_id, SIGINT);
      }
    }
  }
}


void handle_sigquit(int sig) {
  job current_job;
  for(int index = 0; index < MAXJOBS; index++){
    current_job = jobs_list[index];
    if(current_job.fg){
      if(current_job.process_status == running){
        kill(current_job.process_id, SIGQUIT);
      }
    }
  }
}


void install_signal_handlers() {
    //struct sigaction act;
    //act.sa_handler = handle_sigchld;
    //act.sa_flags = SA_RESTART;
    signal(SIGCHLD, handle_sigchld);
    signal(SIGINT, handle_sigint);
    signal(SIGQUIT, handle_sigquit);
    signal(SIGSTOP, handle_sigtstp);

}

void spawn(const char **toks, bool bg) { // bg is true iff command ended with &
  if (active_jobs > MAXJOBS){
    fprintf(stderr, "ERROR: too many jobs");
  }  
  install_signal_handlers();
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGCHLD);
  sigaddset(&mask, SIGSTOP);
  sigaddset(&mask, SIGQUIT);
  sigaddset(&mask, SIGINT);
        
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
    new_job.fg = !bg;
    sprintf(new_job.exit_msg, "[%d] (%d)  finished %s\n", new_job.process_number, new_job.process_id, new_job.process_name);
    sprintf(new_job.kill_msg, "[%d] (%d)  killed %s\n", new_job.process_number, new_job.process_id, new_job.process_name);
    sprintf(new_job.core_dumpd_msg, "[%d] (%d)  killed (core dumped)  %s\n", new_job.process_number, new_job.process_id, new_job.process_name);
    sprintf(new_job.suspend_msg, "[%d] (%d)  suspended %s\n", new_job.process_number, new_job.process_id, new_job.process_name);
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
    if(bg){
      printf("[%d]  (%d)  running  %s \n", num_jobs, new_pid, new_job.process_name);
    }
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
    if(!bg){
      while(waitpid(new_job.process_id, NULL, WNOHANG | WUNTRACED) == 0){
      sleep(.001);
      }
    }
      return;
  } 
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
    if(toks[1] == NULL){
    	//nuke all processes
	//how do I deal with an error in the kill fn
	//make sure to check that there are no more than 1 argument passed
    }
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
