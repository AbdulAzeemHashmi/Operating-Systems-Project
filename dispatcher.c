#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<fcntl.h>
#include<sys/mman.h>
#include<signal.h>
#include<sys/stat.h>
#include<semaphore.h>
#include "common/common.h"

int shm_fd;
void* shm_ptr;
sem_t* done_sem;

pid_t pid_ingester  = -1;
pid_t pid_processor = -1;
pid_t pid_reporter  = -1;
volatile int children_done = 0;

void handle_sigusr1(int sig){
    (void)sig;
    printf("Reporter finished! Report is ready!\n");
}
void handle_sigterm(int sig){
    (void)sig;
    printf("\nSIGTERM received! Cleaning up...\n");
    kill(pid_ingester,  SIGTERM);
    kill(pid_processor, SIGTERM);
    kill(pid_reporter,  SIGTERM);
    unlink(FIFO_PATH);
    munmap(shm_ptr, 4096);
    shm_unlink(SHM_NAME);
    sem_close(done_sem);
    sem_unlink(SEM_NAME);
    exit(143);   // exit code for SIGTERM!
}
void handle_sigchld(int sig){
    (void)sig;
    int status;
    pid_t pid;
    // loop used because multiple SIGCHLDs can merge into one
    while((pid = waitpid(-1, &status, WNOHANG)) > 0){
        children_done++;
        if(pid == pid_ingester)
            printf("Ingester  PID=%d exited status=%d\n", pid, WEXITSTATUS(status));
        else if(pid == pid_processor)
            printf("Processor PID=%d exited status=%d\n", pid, WEXITSTATUS(status));
        else if(pid == pid_reporter)
            printf("Reporter  PID=%d exited status=%d\n", pid, WEXITSTATUS(status));
    }
}
void handle_sigint(int sig){
    (void)sig;
    printf("\nCtrl+C! Cleaning up...\n");
    
    // Kill all children
    kill(pid_ingester,  SIGTERM);
    kill(pid_processor, SIGTERM);
    kill(pid_reporter,  SIGTERM);
    
    // Cleanup IPC
    unlink(FIFO_PATH);
    munmap(shm_ptr, 4096);
    shm_unlink(SHM_NAME);
    sem_close(done_sem);
    sem_unlink(SEM_NAME);
    
    exit(130);  // exit code for SIGINT!
}

int main(int argc, char* argv[]){
    fprintf(stderr, "Dispatcher Running... \nPID=%d \t PPID=%d\n", getpid(), getppid());
    if(argc < 5){
      fprintf(stderr, "Usage: ./dispatcher input_dir output_dir N Q\n");
      return 10;
    }
    char* input_dir  = argv[1];   // "data/"
    char* output_dir = argv[2];   // "output/"
    char* N          = argv[3];   // no. of threads
    char* Q          = argv[4];   // queue size
    
    
  char* ingester_args[] = {
      "./ingester",
      input_dir,          // input_dir
      "/tmp/my_fifo",   // fifo path
      NULL             
  };
  char* processor_args[] = {
      "./processor",
      "/tmp/my_fifo",   // fifo path
      "/my_shm",        // shared memory
      "/my_sem",        // semaphore
      N,               // threads 
      Q,               // queue size 
      NULL
  };
  char* reporter_args[] = {
      "./reporter",
      "/my_shm",        // shared memory
      "/my_sem",        // semaphore
      output_dir,        // output dir
      NULL
  };

  signal(SIGINT, handle_sigint);
  signal(SIGUSR1, handle_sigusr1);
  signal(SIGTERM, handle_sigterm);
  signal(SIGCHLD, handle_sigchld);
  
  mkfifo(FIFO_PATH, 0666);
  printf("FIFO created Successfully\n");
      
  shm_fd = shm_open(SHM_NAME, O_CREAT|O_RDWR, 0666);
  if(shm_fd == -1){
      perror("shm_open failed");
      return 20;
  }
  ftruncate(shm_fd, sizeof(SharedData));
  shm_ptr = mmap(NULL, sizeof(SharedData), PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0);
  if(shm_ptr == MAP_FAILED){
      perror("mmap failed");
      return 20;
  }
  printf("Shared Memory Created!\n");
  
  done_sem = sem_open(SEM_NAME, O_CREAT, 0666, 0);
  if(done_sem == SEM_FAILED){
      perror("Semaphore creation failed");
      return 20;
  }
  printf("Semaphore Created!\n");
  mkdir("logs", 0755);
  
  pid_ingester = fork();
  if(pid_ingester == 0){
      // redirect stdout/stderr to log file!
      int log = open("logs/ingester.log", O_WRONLY|O_CREAT|O_TRUNC, 0666);
      dup2(log, STDOUT_FILENO);
      dup2(log, STDERR_FILENO);
      close(log);
      execvp("./ingester", ingester_args);
      exit(40);
  }

  pid_processor = fork();
  if(pid_processor == 0){
      int log = open("logs/processor.log", O_WRONLY|O_CREAT|O_TRUNC, 0666);
      dup2(log, STDOUT_FILENO);
      dup2(log, STDERR_FILENO);
      close(log);
      execvp("./processor", processor_args);
      exit(40);
  }
  
  pid_reporter = fork();
  if(pid_reporter == 0){
      int log = open("logs/reporter.log", O_WRONLY|O_CREAT|O_TRUNC, 0666);
      dup2(log, STDOUT_FILENO);
      dup2(log, STDERR_FILENO);
      close(log);
      execvp("./reporter", reporter_args);
      exit(40);
  }
  
  int status;
  waitpid(pid_ingester,  &status, 0);
  printf("Ingester  PID=%d exited with status=%d\n", pid_ingester, WEXITSTATUS(status));

  waitpid(pid_processor, &status, 0);
  printf("Processor PID=%d exited with status=%d\n", pid_processor, WEXITSTATUS(status));

  waitpid(pid_reporter,  &status, 0);
  printf("Reporter  PID=%d exited with status=%d\n", pid_reporter, WEXITSTATUS(status));
  
  printf("All Child Processes Finished.\n");  
  
  unlink(FIFO_PATH);          // delete FIFO
  munmap(shm_ptr, 4096);      // unmap memory
  shm_unlink(SHM_NAME);       // delete shared memory
  sem_close(done_sem);        // close semaphore
  sem_unlink(SEM_NAME);       // delete semaphore
  printf("Cleanup Done!\n");
  return 0;
}

