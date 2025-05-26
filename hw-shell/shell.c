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
#define PIPE_MAX 2048
#define STRING_BUFFER_SIZE 4096

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

char current_dir[PATH_MAX];

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

void disable_echoctl(){
  struct termios term;
  tcgetattr(STDIN_FILENO,&term);
  term.c_lflag &=~ ECHOCTL; //关闭echoctl
  tcsetattr(STDIN_FILENO,TCSANOW,&term);
}

void enable_echoctl(){
  struct termios term;
  tcgetattr(STDIN_FILENO,&term);
  term.c_lflag |= ECHOCTL; //开启echoctl
  tcsetattr(STDIN_FILENO,TCSANOW,&term);
}

int cmd_exit(struct tokens* tokens);
int cmd_help(struct tokens* tokens);
int cmd_pwd(struct tokens* tokens);
int cmd_cd(struct tokens* tokens);

char* find_true_path(char* cmd);

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

char* find_true_path(char* cmd){
  char pwd_path[PATH_MAX];
  if(getcwd(pwd_path,PATH_MAX)==NULL){
    perror("getcwd: error");
    return NULL;
  }

  bool is_absulute=false;
  if(access(cmd,X_OK)==0){
    is_absulute=true;
  }

  bool is_found=false;
  if(!is_absulute){
        //首先从当前目录下找可执行文件
    char _full_path[2*PATH_MAX];
    snprintf(_full_path,2*PATH_MAX,"%s/%s",pwd_path,cmd);
    if(access(_full_path,X_OK)==0){
      is_found=true;
      cmd=_full_path;
    }

        //如果找不到，再从PATH环境变量中寻找
    if(!is_found){
      const char* path=getenv("PATH");
      if(path==NULL){
        perror("getenv: error");
        return NULL;
      }
      char* path_copy=strdup(path);
      char* saveptr;
      char* token=strtok_r(path_copy,":",&saveptr);
      while(token!=NULL){
        char full_path[2*PATH_MAX];
        snprintf(full_path,2*PATH_MAX,"%s/%s",token,cmd);
        if(access(full_path,X_OK)==0){
          is_found=true;
          cmd=full_path;
          break;
        }
        token=strtok_r(NULL,":",&saveptr);
      }
      free(path_copy);
    }
  }
  char* true_path=malloc(strlen(cmd)+1);
  if(true_path==NULL){
    perror("malloc: error");
    return NULL;
  }
  strcpy(true_path,cmd);
  if(is_absulute||is_found){
    return true_path;
  }else{
    free(true_path);
    return NULL;
  }
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
  disable_echoctl();
  struct sigaction sa;
  sa.sa_handler = SIG_IGN;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGTSTP, &sa, NULL);//
  static char line[4096];
  // int line_num = 0;

  if(getcwd(current_dir,PATH_MAX)==NULL){
    perror("getcwd: error");
    return -1;
  }
  /* Please only print shell prompts when standard input is not a tty */
  if (shell_is_interactive)
    fprintf(stdout, "%s$ ", current_dir);

  while (fgets(line, 4096, stdin)) {
    /* Split our line into words. */
    struct tokens* tokens = tokenize(line);

    /* Find which built-in function to run. */
    int fundex = lookup(tokens_get_token(tokens, 0));

    if (fundex >= 0) {
      cmd_table[fundex].fun(tokens);
    }else if(tokens_get_token(tokens,0)==NULL){

    } 
    else{
      /* REPLACE this to run commands as programs. */
      // fprintf(stdout, "This shell doesn't know how to run programs.\n");
      //应该先从当前目录下找可执行文件
      //如果找不到，再从PATH环境变量中找
      //如果还是找不到，报错
      //对于管道来说 | 为分割符号,分割的每一块都可以有重定向
      //管道的起始只能有input而不能有output
      //管道的结束只能有output而不能有input
      //中间的管道不能有input和output
      size_t pipe_count=0;
      size_t pipe_index[PIPE_MAX];
      size_t length=tokens_get_length(tokens);

      for(size_t i=0;i<length;++i){
        char *token=tokens_get_token(tokens,i);
        if(strcmp(token,"|")==0){
          pipe_index[pipe_count++]=i;
        }
      }
      pipe_index[pipe_count]=length;
      size_t pre_pipe_index=0;
      char *args[pipe_count+1][STRING_BUFFER_SIZE];
      char *redirection_input=NULL;
      char *redirection_output=NULL;
      //此处是处理管道的
      if(pipe_count==0){
        size_t k=0;
        for(size_t i=0;i<length;++i){
          char* token=tokens_get_token(tokens,i);
          if(strcmp(token,">")==0){
            redirection_output=tokens_get_token(tokens,++i);
          }else if(strcmp(token,"<")==0){
            redirection_input=tokens_get_token(tokens,++i);
          }else{
            args[0][k++]=tokens_get_token(tokens,i);
          }
        }
        args[0][k]=NULL;
      }else{
        for(size_t i=0;i<pipe_count+1;++i){
        size_t index=pipe_index[i];
        size_t k=0;
        for(size_t j=pre_pipe_index;j<index;++j){
          char *token=tokens_get_token(tokens,j);
          if(strcmp(token,"<")==0){
            if(i!=0){
              perror("redirection: error");
              goto end_of_while;//这里的goto是为了不执行后面的代码,重新开始一个循环
            }
            redirection_input=tokens_get_token(tokens,++j);
          }else if(strcmp(token,">")==0){
            if(i!=pipe_count){
              perror("redirection: error");
              goto end_of_while;
            }
            redirection_output=tokens_get_token(tokens,++j);
          }else{
            args[i][k++]=token;
          }
        }
        args[i][k]=NULL;
        pre_pipe_index=index+1;
      }
      }
      
      int pipe_fd[pipe_count+1][2];//防止pipe_count为0时出现错误
      for(size_t i=0;i<pipe_count;++i){
        if(pipe(pipe_fd[i])==-1){
          perror("pipe: error");
          return -1;
        }
      }
      pid_t pid[pipe_count+1];

      for(size_t i=0;i<pipe_count+1;++i){
        args[i][0]=find_true_path(args[i][0]);
        if(args[i][0]==NULL){
          printf("no such command\n");
          for(size_t j=0;j<i;++j){
            free(args[j][0]);//堆内存释放
          }
          goto end_of_while;
        }
      }

      for(size_t i=0;i<pipe_count+1;++i){
        pid[i]=fork();
        if(pid[i]==0){
          if(i==0){
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
            if(pipe_count!=0){
              dup2(pipe_fd[i][1],STDOUT_FILENO);
            }
          }else if(i==pipe_count){
            if(redirection_output!=NULL){
              int fd_out=open(redirection_output,O_WRONLY|O_CREAT|O_TRUNC,0644);
              if(fd_out==-1){
                perror("open: error");
                exit(1);
              }
              dup2(fd_out,STDOUT_FILENO);
              close(fd_out);
            }
            dup2(pipe_fd[i-1][0],STDIN_FILENO);
          }else{
            dup2(pipe_fd[i-1][0],STDIN_FILENO);
            dup2(pipe_fd[i][1],STDOUT_FILENO);
          }
        for(size_t j=0;j<pipe_count;++j){
          if(j!=i-1)close(pipe_fd[j][0]);
          if(j!=i)close(pipe_fd[j][1]);
        }
          execv(args[i][0],args[i]);
          perror("execv: error");
          exit(1);
        }else if(pid[i]>0){
          waitpid(pid[i],NULL,0);
        }else{
          perror("fork: error");
          return -1;
        }


      }
      //fork实际上是复制了父进程的所有文件描述符，不会影响到父进程
      //关闭父进程的管道文件描述符
      for(size_t i=0;i<pipe_count;++i){
        close(pipe_fd[i][0]);
        close(pipe_fd[i][1]);
      }
      for(size_t i = 0;i<pipe_count+1;++i){
        free(args[i][0]);
      }
    }

    if(getcwd(current_dir, PATH_MAX)==NULL){
      perror("getcwd: error");
      return -1;
    }
    end_of_while:
    if (shell_is_interactive)
      /* Please only print shell prompts when standard input is not a tty */
      fprintf(stdout, "%s$ ", current_dir);

    /* Clean up memory */
    tokens_destroy(tokens);

  }

  return 0;
}
