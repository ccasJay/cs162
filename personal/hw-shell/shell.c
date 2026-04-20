#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "tokenizer.h"

/* Convenience macro to silence compiler warnings about unused function parameters. */
#define unused __attribute__((unused))

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

int cmd_exit(struct tokens* tokens);
int cmd_help(struct tokens* tokens);
int cmd_pwd(struct tokens* tokens);
int cmd_cd(struct tokens* tokens);

/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens* tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
  cmd_fun_t* fun;
  char* cmd;
  char* doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {    //register the command
    {cmd_help, "?", "show this help menu"},
    {cmd_exit, "exit", "exit the command shell"},
    {cmd_pwd, "pwd"," prints the current working directory to standard output"},
    {cmd_cd, "cd", "changes the current working directory to that directory"},
};

/* Prints a helpful description for the given command */
int cmd_help(unused struct tokens* tokens) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
  return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens* tokens) { exit(0); }

/* prints the current working directory to standard output*/
int cmd_pwd(unused struct tokens* tokens){
  char cwd[4096];
  if(getcwd(cwd, sizeof(cwd)) != NULL){
    printf("%s\n", cwd);
  }else{
    perror("getcwd() error");
    return -1;
  }
  return 0;
}

/*changes the current working directory to that directory*/
int cmd_cd(struct tokens* tokens){
  char* path = tokens_get_token(tokens, 1);
  if(path == NULL){
    fprintf(stderr, "cd: expected argument\n");
    return -1;
  }
  if(chdir(path) != 0){
    perror("cd error");
    return -1;
  }
  return 0;
}

/* Looks up the built-in command, if it exists. */
int lookup(char cmd[]) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
      return i;
  return -1;
}

/* Intialization procedures for this shell */
void init_shell() {
  /* Our shell is connected to standard input. */
  shell_terminal = STDIN_FILENO;

  /* Check if we are running interactively */
  shell_is_interactive = isatty(shell_terminal);

  /* Always initialize shell_pgid */
  shell_pgid = getpid();

  if (shell_is_interactive) {
    /* If the shell is not currently in the foreground, we must pause the shell until it becomes a
     * foreground process. We use SIGTTIN to pause the shell. When the shell gets moved to the
     * foreground, we'll receive a SIGCONT. */
    while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
      kill(-shell_pgid, SIGTTIN);

    /* Saves the shell's process id */
    shell_pgid = getpid();

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);

    /* Save the current termios to a variable, so it can be restored later. */
    tcgetattr(shell_terminal, &shell_tmodes);
  }
  signal(SIGINT, SIG_IGN); //ignore the ctrl-C signal
  signal(SIGTSTP,SIG_IGN); //ignore the ctrl-Z signal
  signal(SIGQUIT,SIG_IGN); //ignore the ctrl-\ signal
  signal(SIGTTOU,SIG_IGN); //ignore the background write/tcsetpgrp signal
  signal(SIGTTIN,SIG_IGN); //ignore the background read signal
}

/*program execution*/
int execute_program(struct tokens* tokens){
  int n = tokens_get_length(tokens);
  if(n == 0){
    return -1;
  }
  bool InRedirection =false;
  bool OutRedirection = false;

  char** argv = calloc(n+1,sizeof(char*));
  for(int i =0;i<n;i++){
    argv[i] = tokens_get_token(tokens,i);
  }
  char *cmd = argv[0];
  char full_path[4096];
  int found = 0;
  char *redirection_file_in = NULL;
  char *redirection_file_out = NULL;
  
  //scan the redirection "<" or ">"
  for(int i =0;i<n;i++){
    if(strcmp(argv[i],"<") == 0 && i+1<n){
      InRedirection = true;
      redirection_file_in= argv[i+1];
      argv[i] = NULL;
      argv[i+1] = NULL;
      i++;
    }else if(strcmp(argv[i],">") == 0){
      OutRedirection = true;
      redirection_file_out = argv[i+1];
      argv[i] = NULL;
      argv[i+1] = NULL;
      i++;
    }
  }
  //scan the /, whether it's already the fullpath
  if(strchr(cmd,'/')!= NULL){
    if(access(cmd,X_OK)==0){
      strncpy(full_path,cmd,sizeof(full_path));
      found = 1;
    }
  }else{
    char* path_env = getenv("PATH");
    char *path_copy = strdup(path_env); //make a copy of PATH
    if(path_copy == NULL)return -1;
    char *saveptr;
    char *token = strtok_r(path_copy,":",&saveptr);
    while(token != NULL){
      snprintf(full_path,sizeof(full_path),"%s/%s",token,cmd);
      if(access(full_path,X_OK) == 0){
        found =1;
        break;
      }
      token = strtok_r(NULL,":",&saveptr);
    }
    free(path_copy);
  }
  //run the program in the child thread
  if(found){
      if(InRedirection){ // in redirection
        int fd = open(redirection_file_in,O_RDONLY);
        if(fd < 0){
          perror("open error");
          exit(1);
        }
        dup2(fd,STDIN_FILENO);
        close(fd);
      }if(OutRedirection){ // out redirection
        int fd = open(redirection_file_out,O_WRONLY|O_CREAT|O_TRUNC,0644); //0644 is the permission for the created file, the owner can read and write, while the group and others can only read
        if(fd < 0){
          perror("open error");
          exit(1);
        }
        dup2(fd,STDOUT_FILENO);
        close(fd);
      }
      execv(full_path,argv);
    }
  free(argv);
  return 0;
}

//whether use pipe
bool is_pipe(struct tokens* tokens){
  int n = tokens_get_length(tokens);
  if(strcmp(tokens_get_token(tokens,0), "|") ==0 || strcmp(tokens_get_token(tokens,n-1), "|") ==0){
    fprintf(stderr, "Error: pipe cannot be the first or last token\n");
    return false;
  }
  for(int i =0;i<n;i++){
    if(strcmp(tokens_get_token(tokens,i),"|") == 0){
      return true;
    }
  }
  return false;
}

/*execute the program using pipe*/
int execute_pipe(struct tokens* tokens){
  int num_procs = tokens_get_length(tokens);
  int pipe_arr[num_procs-1][2];
  //pre-create the pipes
  for(int i =0;i<num_procs-1;i++){
    pipe(pipe_arr[i]);
  }
  
  // fork loop
  pid_t c_pid, first_child_pgid = -1;
  for(int i =0;i<num_procs;i++){
     c_pid = fork();
    // set the pgid of the child process to itself, so that they can be in the same process
    if(c_pid == 0){
      if(i == 0){
        // First child creates the process group
        setpgrp();
      } else {
        // Subsequent children join the first child's process group
        setpgid(0, first_child_pgid);
      }
      
      signal(SIGINT, SIG_DFL); //restore the default signal handler for ctrl-C
      signal(SIGTSTP,SIG_DFL); //restore the default signal handler for ctrl-Z
      signal(SIGQUIT,SIG_DFL); //restore the default signal handler for ctrl-\. 
      signal(SIGTTOU,SIG_DFL);
      signal(SIGTTIN,SIG_DFL);
      
      //edeg case
      if(i == 0)
      {
        dup2(pipe_arr[0][1],STDOUT_FILENO);
      }
      if(i == num_procs-1)
      {
         dup2(pipe_arr[i][0],STDIN_FILENO);
      }else // change the stdin and stdout for the middle processes
      {
        dup2(pipe_arr[i-1][0],STDIN_FILENO);
        dup2(pipe_arr[i][1],STDOUT_FILENO);
      }
      //close all the FDs in the child process
      for(int j =0;j<num_procs-1;j++){
        close(pipe_arr[j][0]);
        close(pipe_arr[j][1]);
      }
    execute_program(tokens);
    }else if(c_pid>0){
      if(i == 0){
        first_child_pgid = c_pid;
        // 父进程也确保子进程在独立进程组中
        if(setpgid(c_pid, c_pid) == -1) {
          perror("parent setpgid in pipe failed");
        }
        if(shell_is_interactive) {
          if(tcsetpgrp(0, first_child_pgid) == -1) {
            perror("tcsetpgrp to pipeline failed");
          }
        }
      } else {
        // 后续子进程加入第一个子进程的进程组
        if(setpgid(c_pid, first_child_pgid) == -1) {
          perror("parent setpgid for later child failed");
        }
      }
    }
  }
  
  //close the FDs in the parent process
  for(int i =0;i<num_procs-1;i++){
    close(pipe_arr[i][0]);
    close(pipe_arr[i][1]);
  }
  
  // Wait for pipeline children; return control if they are stopped by Ctrl-Z.
  int status;
  pid_t wpid;
  while((wpid = waitpid(-first_child_pgid, &status, WUNTRACED)) > 0) {
    if (WIFSTOPPED(status)) {
      break;
    }
  }
  
  // restore shell to foreground
  if(shell_is_interactive) {
    tcsetpgrp(0, shell_pgid);
  }
  
  return 0;
}


int main(unused int argc, unused char* argv[]) {
  init_shell();

  static char line[4096];
  int line_num = 0;

  /* Please only print shell prompts when standard input is not a tty */
  if (shell_is_interactive)
    fprintf(stdout, "%d: ", line_num);

  while (fgets(line, 4096, stdin)) {
    /* Split our line into words. */
    struct tokens* tokens = tokenize(line);

    /* Find which built-in function to run. */
    int fundex = lookup(tokens_get_token(tokens, 0));

    if (fundex >= 0) {
      cmd_table[fundex].fun(tokens);
    }else if(is_pipe(tokens)){
      execute_pipe(tokens);
    }
    else {
      /* REPLACE this to run commands as programs. */
      pid_t c_pid;
      c_pid = fork();
      if(c_pid == 0){
        // 子进程
        if(setpgrp() == -1) {
          perror("setpgrp failed");
          exit(1);
        }
        signal(SIGINT, SIG_DFL); //restore the default signal handler for ctrl-C
        signal(SIGTSTP,SIG_DFL); //restore the default signal handler for ctrl-Z
        signal(SIGQUIT,SIG_DFL); //restore the default signal handler for ctrl-\ .
        signal(SIGTTOU,SIG_DFL);
        signal(SIGTTIN,SIG_DFL);
        execute_program(tokens);
        exit(0);
      }else if(c_pid > 0){
        // 父进程也要确保子进程在独立进程组中，避免竞态条件
        if(setpgid(c_pid, c_pid) == -1) {
          perror("parent setpgid failed");
        }
        if(shell_is_interactive) {
          if(tcsetpgrp(0, c_pid) == -1) {
            perror("tcsetpgrp to child failed");
          }
        }
        int status;
        pid_t wait_result;
        do {
          wait_result = waitpid(c_pid, &status, WUNTRACED);
        } while (wait_result == -1 && errno == EINTR);
        if (wait_result == -1) {
          perror("waitpid failed");
        }
        if(shell_is_interactive) {
          if(tcsetpgrp(0, shell_pgid) == -1) {
            perror("tcsetpgrp to shell failed");
          }
        }
      }else {
        perror("fork error");
      }
    }

    if (shell_is_interactive)
      /* Please only print shell prompts when standard input is not a tty */
      fprintf(stdout, "%d: ", ++line_num);

    /* Clean up memory */
    tokens_destroy(tokens);
  }
  return 0;
}
