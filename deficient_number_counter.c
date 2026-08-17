// Deficient Number Counter
//
// Scans every ".txt" file in a given directory. For each file, a worker
// thread is spawned that counts how many integers in that file are
// "deficient" numbers (the sum of a number's proper divisors is less than
// the number itself). The number of worker threads active at the same time
// is bounded by a counting semaphore, so at most `threadNumber` files are
// processed concurrently.
//
// Usage: ./deficient_number_counter <directoryName> <threadNumber>

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <dirent.h>
#include <string.h>

#define INITIAL_THREAD_CAPACITY 50
#define FILE_EXTENSION ".txt"

// Limits how many worker threads may run at the same time, like the
// capacity of an instructor's office that can host at most N students.
static sem_t active_threads_sem;

// Data handed off to each worker thread.
typedef struct {
    char filepath[512];
    char filename[256];
    int thread_id;
} ThreadTask;

// Returns 1 if n is a deficient number (sum of its proper divisors is less
// than n), 0 otherwise. Runs in O(sqrt(n)) instead of the naive O(n).
static int is_deficient(int n) {
    if (n <= 0) {
        return 0;
    }

    int divisor_sum = (n == 1) ? 0 : 1; // 1 is a proper divisor of every n > 1

    for (int i = 2; (long)i * i <= n; i++) {
        if (n % i == 0) {
            int complement = n / i;
            divisor_sum += i;
            if (complement != i) {
                divisor_sum += complement;
            }
        }
    }

    return divisor_sum < n;
}

// Counts deficient numbers among the integers stored in `filepath`.
// Returns -1 if the file could not be opened.
static int count_deficient_numbers_in_file(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    if (!file) {
        perror("Failed to open file");
        return -1;
    }

    int number;
    int deficient_count = 0;
    while (fscanf(file, "%d", &number) == 1) {
        if (is_deficient(number)) {
            deficient_count++;
        }
    }

    fclose(file);
    return deficient_count;
}

// Worker entry point: processes one file, reports the result, then frees
// its task and releases its slot in the active-thread semaphore.
static void* process_file(void* arg) {
    ThreadTask* task = (ThreadTask*)arg;

    int deficient_count = count_deficient_numbers_in_file(task->filepath);
    if (deficient_count >= 0) {
        printf("Thread %d has found %d deficient numbers in %s\n",
               task->thread_id, deficient_count, task->filename);
    }

    sem_post(&active_threads_sem);
    free(task);
    return NULL;
}

// Returns non-zero if `name` ends with the ".txt" extension.
static int has_txt_extension(const char* name) {
    size_t name_len = strlen(name);
    size_t ext_len = strlen(FILE_EXTENSION);
    if (name_len < ext_len) {
        return 0;
    }
    return strcmp(name + name_len - ext_len, FILE_EXTENSION) == 0;
}

// Builds a task for `filename` and starts a worker thread for it, growing
// `*threads`/`*capacity` as needed. Returns 0 on success, -1 on failure.
static int spawn_worker(const char* dir_name, const char* filename, int thread_id,
                         pthread_t** threads, int* thread_count, int* capacity) {
    ThreadTask* task = malloc(sizeof(ThreadTask));
    if (!task) {
        perror("Failed to allocate thread task");
        return -1;
    }
    snprintf(task->filepath, sizeof(task->filepath), "%s/%s", dir_name, filename);
    snprintf(task->filename, sizeof(task->filename), "%s", filename);
    task->thread_id = thread_id;

    if (*thread_count >= *capacity) {
        int new_capacity = *capacity * 2;
        pthread_t* resized = realloc(*threads, new_capacity * sizeof(pthread_t));
        if (!resized) {
            perror("Failed to grow thread array");
            free(task);
            return -1;
        }
        *threads = resized;
        *capacity = new_capacity;
    }

    if (pthread_create(&(*threads)[*thread_count], NULL, process_file, task) != 0) {
        perror("Failed to create thread");
        free(task);
        return -1;
    }

    (*thread_count)++;
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <directoryName> <threadNumber>\n", argv[0]);
        return 1;
    }

    const char* dir_name = argv[1];
    int thread_limit = atoi(argv[2]);
    if (thread_limit <= 0) {
        fprintf(stderr, "Error: threadNumber must be greater than 0.\n");
        return 1;
    }

    sem_init(&active_threads_sem, 0, thread_limit);

    DIR* dir = opendir(dir_name);
    if (!dir) {
        perror("Failed to open directory");
        sem_destroy(&active_threads_sem);
        return 1;
    }

    int capacity = INITIAL_THREAD_CAPACITY;
    pthread_t* threads = malloc(capacity * sizeof(pthread_t));
    if (!threads) {
        perror("Failed to allocate thread array");
        closedir(dir);
        sem_destroy(&active_threads_sem);
        return 1;
    }

    int thread_count = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (!has_txt_extension(entry->d_name)) {
            continue;
        }

        // Blocks here until a running worker finishes and posts back,
        // capping the number of simultaneously active threads.
        sem_wait(&active_threads_sem);

        if (spawn_worker(dir_name, entry->d_name, thread_count + 1,
                          &threads, &thread_count, &capacity) != 0) {
            sem_post(&active_threads_sem); // Give back the slot we reserved.
        }
    }
    closedir(dir);

    // Wait for every spawned worker to finish before exiting.
    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    sem_destroy(&active_threads_sem);
    return 0;
}
