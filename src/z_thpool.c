
#include "z_tool.h"
#include "z_kfifo.h"
#include "z_debug.h"
#include "z_thpool.h"
#include "z_table_print.h"

#include <pthread.h>

#define Z_THPOOL_VERION "0.0.1.0"

// Structure to hold thread pool message details
struct z_thpool_msg_struct {
    void (*cb)(void *); // Callback function
    void *p_arg;        // Argument to the callback function
};

// Structure for managing the thread pool
struct z_thpool_mng_struct {
    int32_t start_flag;                     // Flag indicating if the thread pool is started
    struct z_kfifo_struct t_info;           // Information management for message queue
    int32_t th_run_flag;                    // Flag controlling thread pool run state
    uint32_t th_run_nums;                   // Number of currently running threads
    uint32_t th_busy_nums;                  // Number of busy threads
    uint32_t max_nums;                      // Maximum number of threads allowed
    uint32_t msg_node_max;                  // Maximum number of message nodes
    pthread_mutex_t mutex;                  // Mutex for synchronization
    pthread_cond_t cond;                    // Condition variable for thread synchronization
    uint32_t pub_bytes;                     // Data published (for statistics)
    uint32_t sub_bytes;                     // Data consumed (for statistics)
    struct z_thpool_config_struct t_config; // Configuration for thread pool
};

// Global variables for thread pool management and synchronization
static struct z_thpool_mng_struct gs_thpool_mng = {0};
static pthread_mutex_t gs_thpool_mutex = PTHREAD_MUTEX_INITIALIZER;
static int32_t z_thpool_cmd(void);

/**
@brief Create a thread, set its attributes, and start it
@param p_pth Pointer to the thread ID
@param func Function to be executed by the thread
@param p_arg Argument passed to the function
@param stack_size Stack size for the thread
@return Status of thread creation, success is 0
*/
static int32_t z_thpool_create_thread(pthread_t *p_pth, void *(*func)(void *), void *p_arg, uint32_t stack_size) {
    int32_t ret;
    pthread_attr_t attr;

    pthread_attr_init(&attr);

    // Set the thread to be detached
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_setstacksize(&attr, stack_size);

#ifndef CFG_PLATFORM_X86
    // Set explicit scheduling if not on X86 platform
    ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    if (ret) {
        fprintf(stderr, "pthread_attr_setinheritsched:%d\n", ret);
        goto error;
    }
#endif

    // Create the thread with specified attributes
    ret = pthread_create(p_pth, &attr, func, p_arg);
    if (ret) {
        fprintf(stderr, "pthread_create:%d\n", ret);
        goto error;
    }

error:
    // Clean up thread attributes
    pthread_attr_destroy(&attr);
    return ret;
}

/**
@brief Read and process messages from the message queue
@param param Pointer to the thread pool management structure
@return No return value
*/
static void z_thpool_msg_read(void *param) {
    struct z_thpool_mng_struct *mng = (struct z_thpool_mng_struct *)param;
    struct z_thpool_msg_struct msg;

    pthread_mutex_lock(&mng->mutex);
    while (mng->th_run_flag && z_kfifo_data_len(&mng->t_info) < sizeof(struct z_thpool_msg_struct)) {
        pthread_cond_wait(&mng->cond, &mng->mutex);
    }

    if (mng->th_run_flag == 0 || z_kfifo_data_len(&mng->t_info) < sizeof(struct z_thpool_msg_struct)) {
        pthread_mutex_unlock(&mng->mutex);
        return;
    }

    // Retrieve message from the queue
    uint32_t len = z_kfifo_out(&mng->t_info, &msg, sizeof(struct z_thpool_msg_struct));
    mng->th_busy_nums++;
    mng->sub_bytes += len;
    pthread_mutex_unlock(&mng->mutex);

    // Execute the callback function for the message
    msg.cb(msg.p_arg);

    pthread_mutex_lock(&mng->mutex);
    mng->th_busy_nums--;
    pthread_mutex_unlock(&mng->mutex);
}

/**
@brief Thread pool processing function
@param param Pointer to the thread pool management structure
@return No return value
*/
static void *z_thpool_proc(void *param) {
    prctl(PR_SET_NAME, "thp");
    struct z_thpool_mng_struct *mng = (struct z_thpool_mng_struct *)param;

    // Continue processing messages as long as the run flag is set
    while (mng->th_run_flag) {
        z_thpool_msg_read(mng);
    }

    // Decrement the run number after processing is complete
    pthread_mutex_lock(&mng->mutex);
    mng->th_run_nums--;
    pthread_mutex_unlock(&mng->mutex);
    return NULL;
}

/**
@brief Add a work task to the thread pool
@param cb Callback function
@param p_arg Argument for the callback function
@return Status, success is 0
*/
int32_t z_thpool_add_work(void (*cb)(void *), void *p_arg) {
    if (!cb || !p_arg) {
        fprintf(stderr, "Invalid parameters\n");
        return -1;
    }

    struct z_thpool_mng_struct *mng = &gs_thpool_mng;
    pthread_mutex_lock(&mng->mutex);
    if (!mng->start_flag) {
        fprintf(stderr, "Thread pool is not started\n");
        goto error;
    }

    // Check if there is enough space in the queue
    if (z_kfifo_space(&mng->t_info) < sizeof(struct z_thpool_msg_struct)) {
        fprintf(stderr, "FIFO overflow\n");
        goto error;
    }

    struct z_thpool_msg_struct msg;
    msg.cb = cb;
    msg.p_arg = p_arg;
    uint32_t len = z_kfifo_in(&mng->t_info, &msg, sizeof(struct z_thpool_msg_struct));
    if (len != sizeof(struct z_thpool_msg_struct)) {
        fprintf(stderr, "Failed to write to FIFO\n");
        goto error;
    }
    mng->pub_bytes += len;
    pthread_cond_signal(&mng->cond);
    pthread_mutex_unlock(&mng->mutex);
    return 0;

error:
    pthread_mutex_unlock(&mng->mutex);
    return -1;
}

/**
@brief Start the thread pool
@param p_config Configuration parameters for the thread pool
@return No return value
*/
int32_t z_thpool_start(struct z_thpool_config_struct *p_config) {
    Z_DEBUG_ENTER();
    uint32_t max_nums;
    uint32_t msg_node_max;
    int32_t ret = -1;

    struct z_thpool_mng_struct *p_mng = &gs_thpool_mng;
    if (p_mng->start_flag) {
        fprintf(stderr, "Thread pool already started\n");
        goto error0;
    }
    memset(p_mng, 0, sizeof(struct z_thpool_mng_struct));

    p_mng->max_nums = p_config->max_thread_nums;
    p_mng->msg_node_max = p_config->msg_node_max;
    uint32_t stack_size = p_config->thread_stack_size;

    max_nums = gs_thpool_mng.max_nums;
    msg_node_max = gs_thpool_mng.msg_node_max;

    // Initialize mutex and condition variable
    pthread_mutex_init(&p_mng->mutex, NULL);
    pthread_cond_init(&p_mng->cond, NULL);

    // Allocate FIFO for message queue
    ret = z_kfifo_malloc(&p_mng->t_info, sizeof(struct z_thpool_msg_struct) * msg_node_max);
    if (ret != 0) {
        fprintf(stderr, "FIFO allocation failed\n");
        goto error1;
    }

    p_mng->th_run_flag = 1;
    p_mng->msg_node_max = msg_node_max;
    p_mng->max_nums = max_nums;

    // Create threads based on the maximum number configured
    for (uint32_t i = 0; i < max_nums; i++) {
        pthread_t tid;
        p_mng->th_run_nums++;
        ret = z_thpool_create_thread(&tid, z_thpool_proc, p_mng, stack_size);
        if (ret != 0) {
            fprintf(stderr, "Thread creation failed\n");
            goto error2;
        }
    }

    p_mng->start_flag = 1;
    ret = 0;
    Z_DEBUG_EXIT(0);
    return ret;

error2:
    // Handle cleanup on thread creation failure
    p_mng->th_run_flag = 0;
    while (p_mng->th_run_nums) {
        pthread_cond_broadcast(&p_mng->cond);
        usleep(10000);
    }
    z_kfifo_free(&p_mng->t_info);
    pthread_cond_destroy(&p_mng->cond);

error1:
    pthread_mutex_destroy(&p_mng->mutex);
error0:
    Z_DEBUG_EXIT(ret);
    return ret;
}

/**
@brief Exit the thread pool, cleaning up resources
@return No return value
*/
void z_thpool_exit(void) {
    struct z_thpool_mng_struct *p_mng = &gs_thpool_mng;
    fprintf(stderr, "Stopping thread pool\n");

    pthread_mutex_lock(&gs_thpool_mutex);
    if (!p_mng->start_flag) {
        fprintf(stderr, "Thread pool is already stopped\n");
        pthread_mutex_unlock(&gs_thpool_mutex);
        return;
    }

    // Set run flag to 0 and wait for threads to finish
    p_mng->th_run_flag = 0;
    while (p_mng->th_run_nums) {
        pthread_cond_broadcast(&p_mng->cond);
        pthread_mutex_unlock(&gs_thpool_mutex);
        usleep(10000);
        pthread_mutex_lock(&gs_thpool_mutex);
    }

    // Free allocated resources and destroy synchronization primitives
    z_kfifo_free(&p_mng->t_info);
    pthread_cond_destroy(&p_mng->cond);
    pthread_mutex_destroy(&p_mng->mutex);
    memset(p_mng, 0, sizeof(struct z_thpool_mng_struct));
    pthread_mutex_unlock(&gs_thpool_mutex);

    fprintf(stderr, "Thread pool stopped\n");
    return;
}

/*-----------------------------------------------------------------------------------*/
/**
 * @brief Display information about all device templates
 *
 * This function traverses all device template channels and outputs detailed information about each channel's devices.
 *
 * @note The way device information is printed can be customized, depending on the implementation of the custom part.
 */
int z_thpool_cmd_shell_show(void) {
    int ret = -1;
    uint32_t max_thpool_nums = 0;
    uint32_t busy_thpool_nums = 0;
    uint32_t max_cache_nums = 0;
    uint32_t busy_cache_nums = 0;
    uint32_t cur_create_thpool_nums = 0;
    pthread_mutex_lock(&gs_thpool_mutex);

    // Gather and print statistics from the thread pool
    max_thpool_nums = gs_thpool_mng.max_nums;
    cur_create_thpool_nums = gs_thpool_mng.th_run_nums;
    busy_thpool_nums = gs_thpool_mng.th_busy_nums;
    max_cache_nums = gs_thpool_mng.msg_node_max;
    busy_cache_nums = z_kfifo_data_len(&gs_thpool_mng.t_info) / sizeof(struct z_thpool_msg_struct);
    z_table_print_title("z_thpool module");
    z_table_print_row("%-18s %s\n", "Ver: ", Z_THPOOL_VERION); // Version number
    z_table_print_border();
    z_table_print_row("%-18s %d\n", "max nums: ", max_thpool_nums);
    z_table_print_row("%-18s %d\n", "create nums: ", cur_create_thpool_nums);
    z_table_print_row("%-18s %d\n", "busy nums:", busy_thpool_nums);
    z_table_print_row("%-18s %d\n", "max cache nums:", max_cache_nums);
    z_table_print_row("%-18s %d\n", "use cache nums:", busy_cache_nums);
    z_table_print_row("%-18s %d\n", "pub_bytes:", gs_thpool_mng.pub_bytes);
    z_table_print_row("%-18s %d\n", "sub_bytes:", gs_thpool_mng.sub_bytes);
    pthread_mutex_unlock(&gs_thpool_mutex);
    ret = 0;

    return ret;
}

/*-----------------------------------------------------------------------------------*/

/**
@brief Task callback function
@param p_arg Parameter
@return No return value
*/
static void z_thpool_test_task_cb(void *p_arg) {
    int32_t num = *((int32_t *)p_arg);
    printf("Task %d is being processed by thread %lu\n", num, pthread_self());
    sleep(1); // Simulate task processing time
}

/**
@brief Test the thread pool functionality
@return Status, success is 0
*/
int32_t z_thpool_test(void) {
    // Configuration for the thread pool extreme test
    struct z_thpool_config_struct t_config = {.max_thread_nums = 100, .msg_node_max = 100, .thread_stack_size = 64 * 1024};

    printf("Starting thread pool extreme test...\n");
    // Initialize the thread pool
    z_thpool_start(&t_config);

    const uint32_t thpool_max_tasks = gs_thpool_mng.msg_node_max;

    // Define task parameters array
    int32_t task_args[thpool_max_tasks];
    for (int32_t i = 0; i < thpool_max_tasks; i++) {
        task_args[i] = i;
    }

    // Add tasks to the thread pool
    for (int32_t i = 0; i < thpool_max_tasks; i++) {
        if (z_thpool_add_work(z_thpool_test_task_cb, &task_args[i]) != 0) {
            fprintf(stderr, "Failed to add task %d\n", i);
        } else {
            printf("Task %d added successfully\n", i);
        }
    }

    // Wait for tasks to be processed
    for (int32_t i = 0; i < 5; i++) {
        printf("Waiting...\r\n");
        sleep(1);
    }

    // Verify data integrity
    for (int32_t i = 0; i < thpool_max_tasks; i++) {
        printf("Task %d argument after processing: %d\n", i, task_args[i]);
    }

    // Destroy the thread pool after completing tasks
    z_thpool_exit();
    printf("Thread pool extreme test completed.\n");
    return 0;
}
