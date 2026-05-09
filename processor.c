#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <signal.h> 
#include "common/common.h"
// Queue
char** queue;        // array of chunks
int    front  = 0;
int    rear   = 0;
int    Q      = 0;   // queue size from args

// Mutexes
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t agg_mutex   = PTHREAD_MUTEX_INITIALIZER;


sem_t sem_empty;
sem_t sem_full;

// FIFO file descriptor
int fifo_fd;

// N threads
int N = 0;

// Aggregation table
UserData agg_table[100];
int user_count = 0;

// Shared memory
SharedData* shm_ptr;
int proc_running = 1;
// Named semaphore (to signal reporter)
sem_t* done_sem;

int find_or_create_user(char* user_id){
    for(int i = 0; i < user_count; i++){
        if(strcmp(agg_table[i].user_id, user_id) == 0){
            return i;  
        }
    }
    strcpy(agg_table[user_count].user_id, user_id);
    agg_table[user_count].total_session = 0;
    agg_table[user_count].total_visits  = 0;
    agg_table[user_count].total_bounces = 0;
    return user_count++;
}

void* worker_thread(void *arg){
  (void)arg;
  fprintf(stderr, "\nWorker Thread Running...\n");
  while(1){
    sem_wait(&sem_full);
    pthread_mutex_lock(&queue_mutex);
    char* chunk = queue[front];
    front = (front + 1) % Q;
    pthread_mutex_unlock(&queue_mutex);
    sem_post(&sem_empty); 

    if(strcmp(chunk, "POISON") == 0){
        free(chunk);
        fprintf(stderr, "Worker got poison pill. Exiting...\n");
        break;
    }
    char* row = strtok(chunk, "\n");
    while(row != NULL){
      
        if(strlen(row) == 0){
            row = strtok(NULL, "\n");
            continue;
        }
      
    char temp[1024];
    strncpy(temp, row, sizeof(temp));
    char* user_id = strtok(temp, ",");
    if(user_id == NULL){
        row = strtok(NULL, "\n");
        continue;
    }

      // Get remaining values
      int values[100];
      int val_count = 0;
      char* token;
      while((token = strtok(NULL, ",")) != NULL){
          values[val_count++] = atoi(token);
      }

      if(val_count == 0){
          row = strtok(NULL, "\n");
          continue;
      }

      // CALCULATE SESSION LENGTH
      int session_length = 0;
      for(int i = 0; i < val_count; i++){
          session_length += values[i];
      }

      // CALCULATE BOUNCE
      // only 1 value = bounce!
      int bounced = (val_count == 1) ? 1 : 0;

      // UPDATE AGGREGATION TABLE
      // lock so only 1 thread updates at a time!
      pthread_mutex_lock(&agg_mutex);

      int idx = find_or_create_user(user_id);
      agg_table[idx].total_session += session_length;
      agg_table[idx].total_visits  += 1;
      agg_table[idx].total_bounces += bounced;

      pthread_mutex_unlock(&agg_mutex);

      fprintf(stderr, "Processed: %s session=%d bounced=%d\n", user_id, session_length, bounced);

      row = strtok(NULL, "\n");
  }

  free(chunk);
}

return NULL;
}
    
void* reader_thread(void* arg){
    (void)arg;
    fprintf(stderr,"\nReader Thread Running...\n");
    ChunkHeader header;
    
    while(proc_running){
      int bytes = read(fifo_fd, &header, sizeof(header));
      if(bytes <= 0) 
          break;
      if(header.chunk_id == -1){
          fprintf(stderr, "EOF chunk received!\n");
          for(int i = 0; i < N; i++){
              char* poison = malloc(1024);
              strcpy(poison, "POISON");

              sem_wait(&sem_empty);
              pthread_mutex_lock(&queue_mutex);
              queue[rear] = poison;
              rear = (rear + 1) % Q;
              pthread_mutex_unlock(&queue_mutex);
              sem_post(&sem_full);

              fprintf(stderr, "Poison pill %d sent!\n", i+1);
          }
          break;
      }
      char* buffer = malloc(header.byte_count + 1);
      read(fifo_fd, buffer, header.byte_count);
      buffer[header.byte_count] = '\0';

      fprintf(stderr, "Reader got chunk %d (%d bytes)\n", header.chunk_id, header.byte_count);

       // PUT IN QUEUE
      sem_wait(&sem_empty);             // wait if queue FULL
      pthread_mutex_lock(&queue_mutex);
      queue[rear] = buffer;
      rear = (rear + 1) % Q;
      pthread_mutex_unlock(&queue_mutex);
      sem_post(&sem_full);              // tell workers item ready
    }

    fprintf(stderr, "Reader thread Done!\n");
    return NULL;
}   



void proc_sigterm(int sig){
    (void)sig;
    proc_running = 0;
}

void proc_sigusr1(int sig){
    (void)sig;
    fprintf(stderr, "Processor stats: users=%d\n", user_count);
}

int main(int argc, char* argv[]){
    if(argc < 6){
        fprintf(stderr, "Usage: ./processor fifo shm sem N Q\n");
        return 10;
    }
    char* fifo_path = argv[1];
    char* shm_name  = argv[2]; 
    char* sem_name  = argv[3];
       N = atoi(argv[4]);
       Q = atoi(argv[5]);
  
    fprintf(stderr, "\nProcessor Running...\n PID=%d \t PPID=%d\n", getpid(), getppid());
    fprintf(stderr, "N=%d threads, Q=%d queue size\n", N, Q);
    fifo_fd = open(fifo_path, O_RDONLY);
    if(fifo_fd == -1){
        perror("FIFO open failed\n");
        return 40;
    }
    fprintf(stderr, "FIFO opened!\n");
    // OPEN SHARED MEMORY
    int shm_fd = shm_open(shm_name, O_RDWR, 0666);
    if(shm_fd == -1){
        perror("shm_open failed\n");
        return 20;
    }
    shm_ptr = (SharedData*)mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if(shm_ptr == MAP_FAILED){
        perror("mmap failed\n");
        return 20;
    }
    fprintf(stderr, "Shared Memory Opened!\n");
    
    done_sem = sem_open(sem_name, 0);  
    if(done_sem == SEM_FAILED){
        perror("sem_open failed");
        return 20;
    }
        
    queue = malloc(Q * sizeof(char*));
    
    sem_init(&sem_empty, 0, Q);  // Q empty slots
    sem_init(&sem_full,  0, 0);  // 0 full slots
    
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 1024 * 1024);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    
    pthread_t workers[N];
    for(int i=0;i<N;i++){
        pthread_create(&workers[i], &attr, worker_thread, NULL);
        fprintf(stderr, "Worker Thread %d Created!\n", i+1);
    }
    pthread_attr_destroy(&attr);
    
    signal(SIGTERM, proc_sigterm);
    signal(SIGUSR1, proc_sigusr1);
    
    pthread_t reader;
    pthread_create(&reader, NULL, reader_thread, NULL);
    fprintf(stderr, "Reader Thread Created!\n");
    
    pthread_join(reader, NULL);         
    fprintf(stderr, "Reader joined!\n");

    for(int i=0;i<N;i++){
      pthread_join(workers[i], NULL);
      fprintf(stderr, "Worker %d Joined!\n", i+1);
    }
    
    // WRITE RESULTS TO SHARED MEMORY
    shm_ptr->user_count = user_count;
    for(int i = 0; i < user_count; i++){
        shm_ptr->users[i] = agg_table[i];
        fprintf(stderr, "User: %s | Session: %d | Visits: %d | Bounces: %d\n",
                agg_table[i].user_id,
                agg_table[i].total_session,
                agg_table[i].total_visits,
                agg_table[i].total_bounces);
    }
    fprintf(stderr, "Results written to shared memory!\n");
    
    sem_post(done_sem);
    fprintf(stderr,"Reporter Notified\n");
    
    // CLEANUP
    free(queue);
    sem_destroy(&sem_empty);
    sem_destroy(&sem_full);
    pthread_mutex_destroy(&queue_mutex);
    pthread_mutex_destroy(&agg_mutex);
    munmap(shm_ptr, sizeof(SharedData));
    close(shm_fd);
    close(fifo_fd);
    sem_close(done_sem);
    
    fprintf(stderr,"Processor going on a 1 Month Leave Now\n");
    return 0;
}
