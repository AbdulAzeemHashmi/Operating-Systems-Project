#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>      
#include <sys/stat.h>
#include <signal.h>
#include "common/common.h"

int running       = 1;
int files_done    = 0;
int chunks_sent   = 0;
int bytes_sent    = 0;

void handle_sigterm(int sig){
    (void)sig;
    running = 0;   // will stop the loop!
}

void handle_sigusr1(int sig){
    (void)sig;
    fprintf(stderr, "Files processed: %d\n", files_done);
    fprintf(stderr, "Chunks sent:     %d\n", chunks_sent);
    fprintf(stderr, "Bytes sent:      %d\n", bytes_sent);
}

int main(int argc, char* argv[]){

    if(argc < 3){
        fprintf(stderr, "Usage: ./ingester input_dir fifo_path\n");
        return 10;
    }
     char* input_dir = argv[1];
     char* fifo_path = argv[2];
     fprintf(stderr, "Ingester Running...\n PID=%d \t PPID=%d\n", getpid(), getppid());
            
    signal(SIGTERM, handle_sigterm);
    signal(SIGUSR1, handle_sigusr1);
    
    
     DIR* dir = opendir(input_dir);
     if(dir == NULL){
        fprintf(stderr,"Error Opening Directory\n");
        return 40;
    }
    fprintf(stderr,"Opening Directory: %s\n", input_dir);
    
    char csv_files[20][256];
    int file_count = 0;
    
    struct dirent* entry;
    while((entry = readdir(dir)) != NULL){
    
        char* file_name = entry->d_name;
        if(strcmp(file_name, ".") == 0)
              continue;
        if(strcmp(file_name, "..") == 0)
              continue;
              
        char* exist = strstr(file_name, ".csv");
        if(exist != NULL && strlen(exist) == 4){
              snprintf(csv_files[file_count], sizeof(csv_files[file_count]), "%s/%s", input_dir, file_name);
              csv_files[file_count][255] = '\0';
              fprintf(stderr, "CSV File(s) Found: %s\n", csv_files[file_count]);
              file_count++;
        }
    }
    closedir(dir);
    
    if(file_count == 0){
        fprintf(stderr,"No CSV Files found in directory %s\n", input_dir);
        return 40;
    }
    fprintf(stderr,"Total CSV Files Found:  %d\n", file_count);
    
    int write_fd = open(fifo_path, O_WRONLY);
    if(write_fd == -1){
        fprintf(stderr, "Error Opening FIFO!\n");
        return 40;
    }
    fprintf(stderr, "FIFO Opened!\n"); 
    
    int chunk_id = 0;
    for(int i=0;i<file_count;i++){
    
    fprintf(stderr, "\nReading File: %s\n", csv_files[i]);

    
    
    FILE* file = fopen(csv_files[i], "r");
    if(!file){
        fprintf(stderr,"Error in Opening the File\n");
        continue;
    }

    fprintf(stderr, "File Opened Successfully!\n");
        
    fprintf(stderr, "Opening FIFO...\n");
    
    char line_chunk[1024];     

    while(fgets(line_chunk, sizeof(line_chunk), file) != NULL && running){
        ChunkHeader header;
        header.chunk_id   = chunk_id++;
        header.byte_count = strlen(line_chunk);
        header.file_id    = i;

        chunks_sent++;
        bytes_sent += strlen(line_chunk);
        write(write_fd, &header, sizeof(header));          
        write(write_fd, line_chunk, strlen(line_chunk));   
    }
    fprintf(stderr, "Ingester Done!\n");
    fclose(file);
  }
  
  ChunkHeader eof;
  eof.chunk_id   = -1;
  eof.byte_count =  0;
  eof.file_id    = -1;
  write(write_fd, &eof, sizeof(eof));
  fprintf(stderr, "EOF chunk sent!\n");

  close(write_fd);    
  return 0;
}
