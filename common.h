// src/common/common.h
// Shared definitions for all components

#ifndef COMMON_H
#define COMMON_H

// ─────────────────────────────────────
// IPC NAMES
// ─────────────────────────────────────
#define FIFO_PATH "/tmp/my_fifo"
#define SHM_NAME  "/my_shm"
#define SEM_NAME  "/my_sem"

// ─────────────────────────────────────
// CHUNK HEADER
// Used by ingester and processor
// ─────────────────────────────────────
typedef struct {
    int chunk_id;
    int byte_count;
    int file_id;
} ChunkHeader;

// ─────────────────────────────────────
// USER DATA
// Used by processor and reporter
// ─────────────────────────────────────
typedef struct {
    char user_id[64];
    int  total_session;
    int  total_visits;
    int  total_bounces;
} UserData;

// ─────────────────────────────────────
// SHARED MEMORY LAYOUT
// Used by processor and reporter
// ─────────────────────────────────────
typedef struct {
    int      user_count;
    UserData users[100];
} SharedData;


#endif
