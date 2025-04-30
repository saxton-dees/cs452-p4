#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include "lab.h" // Include the header file provided

/**
 * @brief The internal structure for the queue.
 */
struct queue
{
    void **buffer;         // Array to store queue elements (pointers)
    int capacity;        // Maximum number of items in the queue
    int size;            // Current number of items in the queue
    int head;            // Index of the next item to dequeue
    int tail;            // Index where the next item will be enqueued
    pthread_mutex_t mutex; // Mutex for synchronizing access to the queue
    pthread_cond_t not_full; // Condition variable for waiting when queue is full
    pthread_cond_t not_empty; // Condition variable for waiting when queue is empty
    bool shutdown;         // Flag to indicate if the queue is shutting down
};

/**
 * @brief Initialize a new queue
 *
 * @param capacity the maximum capacity of the queue
 * @return A fully initialized queue
 */
// In lab.c

queue_t queue_init(int capacity)
{
    // Check for valid capacity
    if (capacity <= 0) {
        fprintf(stderr, "Error: Queue capacity must be positive.\n");
        return NULL;
    }

    // Allocate memory for the queue structure
    queue_t q = (queue_t)malloc(sizeof(struct queue));
    if (!q)
    {
        perror("Failed to allocate queue structure");
        return NULL;
    }

    // Allocate memory for the buffer inside the queue
    // Note: malloc(0) behavior can be implementation-defined, might return NULL
    // or a unique pointer. Explicitly disallowing capacity <= 0 avoids this ambiguity.
    q->buffer = (void **)malloc(capacity * sizeof(void *));
    if (!q->buffer)
    {
        perror("Failed to allocate queue buffer");
        free(q); // Clean up queue structure allocation
        return NULL;
    }

    // Initialize queue properties
    q->capacity = capacity;
    q->size = 0;
    q->head = 0;
    q->tail = 0;
    q->shutdown = false;

    // Initialize mutex and condition variables
    if (pthread_mutex_init(&q->mutex, NULL) != 0)
    {
        perror("Mutex initialization failed");
        free(q->buffer);
        free(q);
        return NULL;
    }
    if (pthread_cond_init(&q->not_full, NULL) != 0)
    {
        perror("Not_full condition variable initialization failed");
        pthread_mutex_destroy(&q->mutex); // Clean up mutex
        free(q->buffer);
        free(q);
        return NULL;
    }
    if (pthread_cond_init(&q->not_empty, NULL) != 0)
    {
        perror("Not_empty condition variable initialization failed");
        pthread_mutex_destroy(&q->mutex); // Clean up mutex
        pthread_cond_destroy(&q->not_full); // Clean up not_full cond var
        free(q->buffer);
        free(q);
        return NULL;
    }

    return q;
}

/**
 * @brief Frees all memory and related data signals all waiting threads.
 *
 * @param q a queue to free
 */
void queue_destroy(queue_t q)
{
    if (!q)
    {
        return; // Nothing to destroy if queue is NULL
    }

    // It's good practice to ensure shutdown is called before destroying,
    // but we'll signal just in case to release any potentially stuck threads.
    // Lock needed to safely modify shutdown and broadcast.
    pthread_mutex_lock(&q->mutex);
    q->shutdown = true; // Ensure shutdown state is set
    // Wake up all waiting threads so they can exit
    pthread_cond_broadcast(&q->not_full);
    pthread_cond_broadcast(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);


    // Destroy mutex and condition variables
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->not_full);
    pthread_cond_destroy(&q->not_empty);

    // Free the buffer and the queue structure
    free(q->buffer);
    free(q);
}

/**
 * @brief Adds an element to the back of the queue
 *
 * @param q the queue
 * @param data the data to add
 */
void enqueue(queue_t q, void *data)
{
    if (!q) return; // Safety check

    pthread_mutex_lock(&q->mutex);

    // Wait while the queue is full AND not shutting down
    while (q->size == q->capacity && !q->shutdown)
    {
        pthread_cond_wait(&q->not_full, &q->mutex);
    }

    // If shutdown was signaled while waiting, just unlock and return
    if (q->shutdown)
    {
        pthread_mutex_unlock(&q->mutex);
        // Note: The item 'data' is not enqueued and might be lost if the caller doesn't handle it.
        // In the context of main.c, producers stop generating items before shutdown, so this is okay.
        return;
    }

    // Add the data to the buffer
    q->buffer[q->tail] = data;
    q->tail = (q->tail + 1) % q->capacity; // Move tail, wrap around if necessary
    q->size++;                             // Increment size

    // Signal that the queue is no longer empty
    pthread_cond_signal(&q->not_empty);

    pthread_mutex_unlock(&q->mutex);
}

/**
 * @brief Removes the first element in the queue.
 *
 * @param q the queue
 * @return The data pointer removed, or NULL if the queue was shutdown and empty.
 */
void *dequeue(queue_t q)
{
    // Safety check for NULL queue
   if (!q) return NULL; // Safety check

    // this check caused the program to core dump
    // if (q->size == 0 && !q->shutdown) {
    //     pthread_mutex_unlock(&q->mutex);
    //     return NULL; // Return NULL if empty and not shutting down
    // }

    pthread_mutex_lock(&q->mutex);

    // Wait while the queue is empty AND not shutting down
    while (q->size == 0 && !q->shutdown)
    {
        pthread_cond_wait(&q->not_empty, &q->mutex);
    }

    // If the queue is shutting down AND is empty, unlock and return NULL
    if (q->shutdown && q->size == 0)
    {
        pthread_mutex_unlock(&q->mutex);
        return NULL; // Indicate shutdown and empty queue
    }

    // Remove the data from the buffer
    void *data = q->buffer[q->head];
    q->head = (q->head + 1) % q->capacity; // Move head, wrap around if necessary
    q->size--;                             // Decrement size

    // Signal that the queue is no longer full
    pthread_cond_signal(&q->not_full);

    pthread_mutex_unlock(&q->mutex);

    return data;
}

/**
 * @brief Set the shutdown flag in the queue so all threads can
 * complete and exit properly
 *
 * @param q The queue
 */
void queue_shutdown(queue_t q)
{
    if (!q) return;

    pthread_mutex_lock(&q->mutex);
    q->shutdown = true;
    // Wake up ALL waiting threads (producers and consumers)
    pthread_cond_broadcast(&q->not_full);
    pthread_cond_broadcast(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
}

/**
 * @brief Returns true if the queue is empty
 * Note: This provides a snapshot. The state could change immediately after.
 * Use with caution in multi-threaded contexts outside the enqueue/dequeue logic.
 * @param q the queue
 */
bool is_empty(queue_t q)
{
    if (!q) return true; // Consider a NULL queue empty

    pthread_mutex_lock(&q->mutex);
    bool empty = (q->size == 0);
    pthread_mutex_unlock(&q->mutex);
    return empty;
}

/**
 * @brief Returns true if the queue is in shutdown mode.
 *
 * @param q The queue
 */
bool is_shutdown(queue_t q)
{
    if (!q) return true; // Consider a NULL queue shutdown

    pthread_mutex_lock(&q->mutex);
    bool shutdown_status = q->shutdown;
    pthread_mutex_unlock(&q->mutex);
    return shutdown_status;
}