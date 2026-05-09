#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "common/common.h"

int main(int argc, char* argv[]){
    if(argc < 4){
        fprintf(stderr, "Usage: ./reporter shm sem output_dir\n");
        return 10;
    }
    char* shm_name = argv[1];
    char* sem_name = argv[2];
    char* output_dir = argv[3];
    SharedData* shm_ptr;
    fprintf(stderr, "\nReporter Running...\n PID=%d \t PPID=%d\n", getpid(), getppid());
    
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
    
    
    sem_t* done_sem = sem_open(sem_name, 0);
    if(done_sem == SEM_FAILED){
        perror("sem_open failed");
        return 20;
    }
    fprintf(stderr, "Semaphore Opened!\n");
    
    
    
    fprintf(stderr, "Reporter waiting for data...\n");
    sem_wait(done_sem);   // ← BLOCKS until processor posts
    fprintf(stderr, "Bell received! Starting report...\n");
    
    fprintf(stderr, "user_count in shm = %d\n", shm_ptr->user_count);
    
    
    int user_count = shm_ptr->user_count;
    fprintf(stderr, "Total users: %d\n", user_count);
    for(int i = 0; i < shm_ptr->user_count; i++){
    fprintf(stderr, "User[%d]: id=%s session=%d visits=%d bounces=%d\n",
            i,
            shm_ptr->users[i].user_id,
            shm_ptr->users[i].total_session,
            shm_ptr->users[i].total_visits,
            shm_ptr->users[i].total_bounces);
    }

    float avg_session[100];
    float bounce_rate[100];

    for(int i = 0; i < user_count; i++){
        // Calculate average session length
        avg_session[i] = (float)shm_ptr->users[i].total_session / shm_ptr->users[i].total_visits;

        // Calculate bounce rate percentage
        bounce_rate[i] = (float)shm_ptr->users[i].total_bounces / shm_ptr->users[i].total_visits * 100;

        fprintf(stderr, "User: %s | Avg: %.1f | Bounce: %.1f%%\n", shm_ptr->users[i].user_id,avg_session[i],bounce_rate[i]);
    }
    
    // Build full path for report.txt
    char txt_path[256];
    snprintf(txt_path, sizeof(txt_path), "%s/report.txt", output_dir);
    
// === dup/dup2 DEMONSTRATION BLOCK START ===
// Step 1: Save current stdout using dup()
// dup() creates a copy of file descriptor 1 (stdout)

    int saved_stdout = dup(1);
    
    // Open report.txt file
    int report_fd = open(txt_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(report_fd == -1){
        perror("report.txt open failed");
        return 40;
    }
    
    // Redirect stdout to report.txt
    // Now ALL printf() goes to file!
    dup2(report_fd, 1);
    
    printf("========================================\n");
    printf("       WEB CLICKSTREAM REPORT           \n");
    printf("========================================\n");
    printf("Total Users Analyzed: %d\n\n", user_count);
    printf("%-10s | %-15s | %-12s\n",
           "User ID", "Avg Session(s)", "Bounce Rate");
    printf("-----------|-----------------|------------\n");

    for(int i = 0; i < user_count; i++){
        printf("%-10s | %-15.1f | %-11.1f%%\n", shm_ptr->users[i].user_id, avg_session[i], bounce_rate[i]);
    }

    printf("========================================\n");
    printf("Report generated successfully!\n");
    fflush(stdout);
    
    // Restore stdout back to terminal
    dup2(saved_stdout, 1);
    close(saved_stdout);
    close(report_fd);
    // === dup/dup2 BLOCK END ===

    fprintf(stderr, "report.txt written!\n");

    // Build full path for report.csv
    char csv_path[256];
    snprintf(csv_path, sizeof(csv_path), "%s/report.csv", output_dir);

    FILE* csv = fopen(csv_path, "w");
    if(!csv){
        perror("report.csv open failed");
        return 40;
    }

    fprintf(csv, "user_id,avg_session,total_visits,bounce_rate\n");

    // Write each user's data
    for(int i = 0; i < user_count; i++){
        fprintf(csv, "%s,%.1f,%d,%.1f\n",
                shm_ptr->users[i].user_id,
                avg_session[i],
                shm_ptr->users[i].total_visits,
                bounce_rate[i]);
    }

    fclose(csv);
    fprintf(stderr, "report.csv written!\n");

    // getppid() = dispatcher's PID

    kill(getppid(), SIGUSR1);
    fprintf(stderr, "Dispatcher notified! (SIGUSR1 sent)\n");

    munmap(shm_ptr, sizeof(SharedData));
    close(shm_fd);
    sem_close(done_sem);

    fprintf(stderr, "Reporter done! Exiting with 0\n");
    return 0;
}    
  
