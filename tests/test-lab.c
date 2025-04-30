#include "harness/unity.h"
#include "../src/lab.h"
#include <stdlib.h> // For malloc/free in some tests if needed
#include <stdio.h>  // For printf in debugging if needed

// NOTE: Due to the multi-threaded nature of this project. Unit testing for this
// project is limited. I have provided you with a command line tester in
// the file app/main.cp. Be aware that the examples below do not test the
// multi-threaded nature of the queue. You will need to use the command line
// tester to test the multi-threaded nature of your queue. Passing these tests
// does not mean your queue is correct. It just means that it can add and remove
// elements from the queue below the blocking threshold.


void setUp(void) {
  // set stuff up here (e.g., if you need common data for tests)
}

void tearDown(void) {
  // clean stuff up here (e.g., free common data)
}

// ::: Existing Tests :::

void test_create_destroy(void)
{
    queue_t q = queue_init(10);
    TEST_ASSERT_NOT_NULL(q); // More specific assertion
    queue_destroy(q);
}

void test_queue_dequeue(void)
{
    queue_t q = queue_init(10);
    TEST_ASSERT_NOT_NULL(q);
    int data = 1;
    enqueue(q, &data);
    TEST_ASSERT_FALSE(is_empty(q)); // Queue should not be empty
    TEST_ASSERT_EQUAL_PTR(&data, dequeue(q)); // Check pointer equality
    TEST_ASSERT_TRUE(is_empty(q));  // Should be empty now
    queue_destroy(q);
}

void test_queue_dequeue_multiple(void)
{
    queue_t q = queue_init(10);
    TEST_ASSERT_NOT_NULL(q);
    int data = 1;
    int data2 = 2;
    int data3 = 3;
    enqueue(q, &data);
    enqueue(q, &data2);
    enqueue(q, &data3);
    TEST_ASSERT_FALSE(is_empty(q));
    TEST_ASSERT_EQUAL_PTR(&data, dequeue(q));
    TEST_ASSERT_EQUAL_PTR(&data2, dequeue(q));
    TEST_ASSERT_EQUAL_PTR(&data3, dequeue(q));
    TEST_ASSERT_TRUE(is_empty(q));
    queue_destroy(q);
}

void test_queue_dequeue_shutdown(void)
{
    queue_t q = queue_init(10);
    TEST_ASSERT_NOT_NULL(q);
    int data = 1;
    int data2 = 2;
    int data3 = 3;
    enqueue(q, &data);
    enqueue(q, &data2);
    enqueue(q, &data3);
    TEST_ASSERT_EQUAL_PTR(&data, dequeue(q));
    TEST_ASSERT_EQUAL_PTR(&data2, dequeue(q));
    TEST_ASSERT_FALSE(is_shutdown(q)); // Not shut down yet
    queue_shutdown(q);
    TEST_ASSERT_TRUE(is_shutdown(q));  // Should be shutdown now
    TEST_ASSERT_EQUAL_PTR(&data3, dequeue(q)); // Dequeue remaining item
    TEST_ASSERT_TRUE(is_empty(q));       // Should be empty after draining
    TEST_ASSERT_NULL(dequeue(q));        // Dequeue from empty+shutdown queue returns NULL
    queue_destroy(q);
}

// ::: New Tests :::

void test_is_empty_initial(void)
{
    queue_t q = queue_init(5);
    TEST_ASSERT_NOT_NULL(q);
    TEST_ASSERT_TRUE(is_empty(q)); // Should be empty initially
    queue_destroy(q);
}

void test_is_empty_after_enqueue_dequeue(void)
{
    queue_t q = queue_init(5);
    TEST_ASSERT_NOT_NULL(q);
    int data = 10;
    enqueue(q, &data);
    TEST_ASSERT_FALSE(is_empty(q));
    dequeue(q);
    TEST_ASSERT_TRUE(is_empty(q));
    queue_destroy(q);
}

void test_fill_to_capacity(void)
{
    int capacity = 5;
    queue_t q = queue_init(capacity);
    TEST_ASSERT_NOT_NULL(q);
    int data[capacity]; // Array to hold data pointers

    // Fill the queue exactly to capacity
    for (int i = 0; i < capacity; i++) {
        data[i] = i;
        enqueue(q, &data[i]);
        TEST_ASSERT_FALSE(is_empty(q));
    }

    // In a single-threaded test, we can't easily check for blocking
    // on the next enqueue. The command-line tester is needed for that.
    // However, we can check that all items are present and in order.

    for (int i = 0; i < capacity; i++) {
        TEST_ASSERT_EQUAL_PTR(&data[i], dequeue(q));
    }
    TEST_ASSERT_TRUE(is_empty(q));
    queue_destroy(q);
}

void test_dequeue_from_empty(void)
{
    queue_t q = queue_init(3);
    TEST_ASSERT_NOT_NULL(q);
    TEST_ASSERT_TRUE(is_empty(q));
    // In a single-threaded test, dequeue on empty might return NULL
    // or have undefined behavior if it expects to block.
    // The spec/implementation determines the correct check.
    // Assuming it returns NULL or doesn't crash.
    // If the queue is designed to block, this test might hang without threads.
    // Let's assume `dequeue` returns NULL if it can't block and finds queue empty.
    // Update this based on your `dequeue` implementation's non-blocking behavior.

    TEST_ASSERT_NULL(dequeue(q)); // Add this line IF dequeue is expected to return NULL when empty and non-blocking

    queue_destroy(q);
}

void test_fifo_order_rigorous(void)
{
    queue_t q = queue_init(100); // Use a larger queue
    TEST_ASSERT_NOT_NULL(q);
    int num_items = 50;
    int data[num_items];

    // Enqueue 50 items
    for(int i = 0; i < num_items; i++) {
        data[i] = i * 10;
        enqueue(q, &data[i]);
    }

    // Dequeue and check order
    for(int i = 0; i < num_items; i++) {
        void* item = dequeue(q);
        TEST_ASSERT_NOT_NULL(item);
        TEST_ASSERT_EQUAL_PTR(&data[i], item); // Verify it's the expected pointer
        TEST_ASSERT_EQUAL_INT(i * 10, *(int*)item); // Verify the value it points to
    }
    TEST_ASSERT_TRUE(is_empty(q));
    queue_destroy(q);
}

void test_interleaved_enqueue_dequeue(void)
{
    queue_t q = queue_init(5);
    TEST_ASSERT_NOT_NULL(q);
    int d1 = 1, d2 = 2, d3 = 3, d4 = 4, d5 = 5;

    enqueue(q, &d1); // q = [1]
    enqueue(q, &d2); // q = [1, 2]
    TEST_ASSERT_EQUAL_PTR(&d1, dequeue(q)); // q = [2]
    enqueue(q, &d3); // q = [2, 3]
    enqueue(q, &d4); // q = [2, 3, 4]
    TEST_ASSERT_EQUAL_PTR(&d2, dequeue(q)); // q = [3, 4]
    enqueue(q, &d5); // q = [3, 4, 5]
    TEST_ASSERT_EQUAL_PTR(&d3, dequeue(q)); // q = [4, 5]
    TEST_ASSERT_EQUAL_PTR(&d4, dequeue(q)); // q = [5]
    TEST_ASSERT_EQUAL_PTR(&d5, dequeue(q)); // q = []
    TEST_ASSERT_TRUE(is_empty(q));
    queue_destroy(q);
}

void test_shutdown_empty_queue(void)
{
    queue_t q = queue_init(5);
    TEST_ASSERT_NOT_NULL(q);
    TEST_ASSERT_TRUE(is_empty(q));
    TEST_ASSERT_FALSE(is_shutdown(q));
    queue_shutdown(q);
    TEST_ASSERT_TRUE(is_shutdown(q));
    TEST_ASSERT_TRUE(is_empty(q)); // Still empty
    TEST_ASSERT_NULL(dequeue(q));   // Dequeue from empty+shutdown queue returns NULL
    queue_destroy(q);
}

void test_enqueue_after_shutdown(void)
{
    queue_t q = queue_init(5);
    TEST_ASSERT_NOT_NULL(q);
    int d1 = 1, d2 = 2;
    enqueue(q, &d1);
    queue_shutdown(q);
    TEST_ASSERT_TRUE(is_shutdown(q));

    // Enqueue after shutdown should ideally be ignored or fail gracefully.
    // The producer threads in main.c stop producing *before* shutdown.
    // Assuming enqueue does nothing after shutdown is set:
    enqueue(q, &d2); // Try to enqueue after shutdown

    // Check that only the item enqueued before shutdown remains
    void* item = dequeue(q);
    TEST_ASSERT_EQUAL_PTR(&d1, item);
    TEST_ASSERT_TRUE(is_empty(q));
    TEST_ASSERT_NULL(dequeue(q)); // Should be empty now

    queue_destroy(q);
}

void test_init_zero_capacity(void) {
  // Test that initializing with zero capacity fails and returns NULL.
  queue_t q = queue_init(0);
  TEST_ASSERT_NULL(q); // Assert that init fails for capacity 0

  // Also test negative capacity
  q = queue_init(-5);
  TEST_ASSERT_NULL(q); // Assert that init fails for negative capacity
}


// ::: Main Test Runner :::

int main(void) {
  UNITY_BEGIN();
  // Existing Tests
  RUN_TEST(test_create_destroy);
  RUN_TEST(test_queue_dequeue);
  RUN_TEST(test_queue_dequeue_multiple);
  RUN_TEST(test_queue_dequeue_shutdown);

  // New Tests
  RUN_TEST(test_is_empty_initial);
  RUN_TEST(test_is_empty_after_enqueue_dequeue);
  RUN_TEST(test_fill_to_capacity);
//   RUN_TEST(test_dequeue_from_empty);
  RUN_TEST(test_fifo_order_rigorous);
  RUN_TEST(test_interleaved_enqueue_dequeue);
  RUN_TEST(test_shutdown_empty_queue);
  RUN_TEST(test_enqueue_after_shutdown);
  RUN_TEST(test_init_zero_capacity);

  return UNITY_END();
}
