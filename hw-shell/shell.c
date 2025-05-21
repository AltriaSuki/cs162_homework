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
#define PATH_MAX 2048

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

fun_desc_t cmd_table[] = {
    {cmd_help, "?", "show this help menu"},
    {cmd_exit, "exit", "exit the command shell"},
    {cmd_pwd,"pwd","print working directory"},
    {cmd_cd,"cd","change directory"},
};

/* Prints a helpful description for the given command */
int cmd_help(unused struct tokens* tokens) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
  return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens* tokens) { exit(0); }

int cmd_pwd(unused struct tokens* tokens){
  char path[PATH_MAX];
  if(getcwd(path,PATH_MAX)==NULL){
    perror("pwd: error");
    return -1;
  }
  printf("%s\n",path);
  return 1;
}

int cmd_cd(struct tokens* tokens){
  if(tokens_get_length(tokens) !=2){
    perror("cd: need exactly one argument");
    return -1;
  }
  char *path=tokens_get_token(tokens,1);
  if(chdir(path)==-1){
    perror("cd: error");
    return -1;
  }
  return 1;
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
    }else {
      /* REPLACE this to run commands as programs. */
      // fprintf(stdout, "This shell doesn't know how to run programs.\n");
      //应该先从当前目录下找可执行文件
      //如果找不到，再从PATH环境变量中找
      //如果还是找不到，报错
      const char* redirection_input=NULL;
      const char* redirection_output=NULL;
      size_t redirection_input_index=0;
      size_t redirection_output_index=0;
      bool has_redirection_input=false;
      bool has_redirection_output=false;
      size_t length=tokens_get_length(tokens);
      for(size_t i=0;i<length;++i){
        char *token=tokens_get_token(tokens,i);
        if(strcmp(token,">")==0){
          redirection_output=tokens_get_token(tokens,i+1);
          redirection_output_index=i;
          has_redirection_output=true;
          if(redirection_output==NULL){
            perror("redirection: error");
            return -1;
          }
        }
        if(strcmp(token,"<")==0){
          redirection_input=tokens_get_token(tokens,i+1);
          redirection_input_index=i;
          has_redirection_input=true;
          if(redirection_input==NULL){
            perror("redirection: error");
            return -1;
          }
        }
        if(has_redirection_input&&has_redirection_output){
          break;
        }
      }
      if(redirection_input_index!=0){
        length-=2;
      }
      if(redirection_output_index!=0){
        length-=2;
      }
      char pwd_path[PATH_MAX];
      if(getcwd(pwd_path,PATH_MAX)==NULL){
        perror("getcwd: error");
        return -1;
      }

      char* args[length+1];
      for(size_t i=0;i<length;i++){
        args[i]=tokens_get_token(tokens,i);
      }
      args[length]=NULL;
      bool is_absulute=false;
      if(access(args[0],X_OK)==0){
        is_absulute=true;
      }
      bool is_found=false;
      if(!is_absulute){
        //首先从当前目录下找可执行文件
        char _full_path[2*PATH_MAX];
        snprintf(_full_path,2*PATH_MAX,"%s/%s",pwd_path,args[0]);
        if(access(_full_path,X_OK)==0){
          is_found=true;
          args[0]=_full_path;
        }

        //如果找不到，再从PATH环境变量中寻找
        if(!is_found){
          const char* path=getenv("PATH");
          if(path==NULL){
            perror("getenv: error");
            return -1;
          }
          char* path_copy=strdup(path);
          char* saveptr;
          char* token=strtok_r(path_copy,":",&saveptr);
          while(token!=NULL){
            char full_path[2*PATH_MAX];
            snprintf(full_path,2*PATH_MAX,"%s/%s",token,args[0]);
            if(access(full_path,X_OK)==0){
              is_found=true;
              args[0]=full_path;
              break;
            }
            token=strtok_r(NULL,":",&saveptr);
          }
          free(path_copy);
        }
      }
      if(is_absulute||is_found){
            pid_t pid=fork();
            if(pid==0){

              if(redirection_input!=NULL){
                int fd_in=open(redirection_input,O_RDONLY);
                if(fd_in==-1){
                  perror("open: error");
                  exit(1);
                }
                dup2(fd_in,STDIN_FILENO);
                close(fd_in);
              }
              if(redirection_output!=NULL){
                int fd_out=open(redirection_output,O_WRONLY|O_CREAT|O_TRUNC,0644);
                if(fd_out==-1){
                  perror("open: error");
                  exit(1);
                }
                dup2(fd_out,STDOUT_FILENO);
                close(fd_out);
              }
              execv(args[0],args);
              perror("execv: error");
              exit(1);
            }else if(pid>0){
              waitpid(pid,NULL,0);
            }else{
              perror("fork: error");
              return -1;
            }
          }else{
            printf("%s command not found\n",args[0]);
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
