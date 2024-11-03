#ifndef _Z_THPOOL_H_
#define _Z_THPOOL_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// Data structure for configuring the thread pool
struct z_thpool_config_struct {
    uint32_t max_thread_nums;   // Maximum number of threads in the pool
    uint32_t msg_node_max;      // Maximum number of message nodes in the pool
    uint32_t thread_stack_size; // Stack size for each thread
};

// Function to add a work task to the thread pool
// @param cb: The callback function that defines the task
// @param arg: The argument to pass to the task
// @return: Returns 0 on success, or a negative error code on failure
int32_t z_thpool_add_work(void (*cb)(void *), void *arg);

// Function to start the thread pool
// @param p_config: Pointer to a configuration structure specifying pool parameters
// @return: Returns 0 on successful start, or a negative error code on failure
int32_t z_thpool_start(struct z_thpool_config_struct *p_config);

// Function to gracefully exit the thread pool, cleaning up resources
void z_thpool_exit(void);

// Function to test the thread pool functionality
// @return: Returns 0 on success, or a negative error code on failure
int32_t z_thpool_test(void);

// Function to display configuration and statistics of the thread pool
// @return: Returns 0 on success
int z_thpool_cmd_shell_show(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _Z_CONFIG_H_ */
