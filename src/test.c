#include "z_tool.h"
#include "z_debug.h"
#include "z_thpool.h"

// Help text for command-line interface
static const char *gs_help =
    "\r\n"
    "Enter a command (type 'exit' to quit):\r\n\r\n"
    "start 5 100 64  #Start with 10 threads, 50 cache queues, set the thread stack 64k size.\r\n"
    "stop            #Stop. \r\n"
    "add 10          #Add 10 threads \r\n"
    "test            #Test thpool \r\n"
    "show            #Show thpool state \r\n"
    "help            #Help \r\n";

// Simulate a task by sleeping for 1 seconds; used in threading tests
static void test_task_cb(void *p_arg) {
    sleep(1); // Simulating task processing time
}

int main(int argc, char *argv[]) {
    char input[256];   // Buffer to hold user input
    char command[256]; // Command read from user input
    int a, b, c;       // Variables for command parameters

    printf("Enter a command (type 'exit' to quit):\n");

    while (1) {
        printf("> ");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            printf("Error reading input. Exiting.\n");
            break;
        }

        // Remove trailing newline character from input
        input[strcspn(input, "\n")] = '\0';

        // Parse the command from the input string
        if (sscanf(input, "%s %d %d %d", command, &a, &b, &c) >= 1) {
            if (strcmp(command, "exit") == 0) {
                // Exit the program
                printf("Exiting program.\n");
                break;
            } else if (strcmp(command, "start") == 0) {
                // Start the thread pool with specified configuration
                struct z_thpool_config_struct t_conftg;
                t_conftg.max_thread_nums = a;
                t_conftg.msg_node_max = b;
                t_conftg.thread_stack_size = c * 1024; // Convert stack size to bytes
                z_thpool_start(&t_conftg);
            } else if (strcmp(command, "stop") == 0) {
                // Stop the thread pool
                z_thpool_exit();
            } else if (strcmp(command, "add") == 0) {
                // Add the specified number of threads using the test task callback
                for (int i = 0; i < a; i++) {
                    if (z_thpool_add_work(test_task_cb, &i)) {
                        break; // Exit loop if adding work fails
                    }
                }
            } else if (strcmp(command, "test") == 0) {
                // Run a test suite for the thread pool
                z_thpool_test();
                break;
            } else if (strcmp(command, "show") == 0) {
                // Display the current state of the thread pool
                z_thpool_cmd_shell_show();
            } else if (strcmp(command, "help") == 0) {
                // Display help information
                printf("%s", gs_help);
            } else {
                // Display help information if command is unknown
                printf("%s", gs_help);
            }
        }
    }
    return 0;
}