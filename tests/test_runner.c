#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/dsc.h"

// Test framework
#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "Test failed: %s\n", msg); \
        return 1; \
    } \
} while(0)

#define TEST_RUN(test) do { \
    printf("Running test: %s\n", #test); \
    if (test() != 0) { \
        fprintf(stderr, "Test %s failed\n", #test); \
        return 1; \
    } \
    printf("Test %s passed\n", #test); \
} while(0)

// Test cases
static int test_command_registration(void) {
    command_t test_cmd = {
        .name = "test",
        .main_func = NULL,
        .fs_func = NULL,
        .use_file = false,
        .is_inferfs = false,
        .expected_filename_index = 0,
        .terminating = false
    };

    TEST_ASSERT(command_register(&test_cmd) == DSC_SUCCESS, "Command registration failed");
    TEST_ASSERT(command_get("test") != NULL, "Command not found after registration");
    TEST_ASSERT(command_unregister("test") == DSC_SUCCESS, "Command unregistration failed");
    TEST_ASSERT(command_get("test") == NULL, "Command still found after unregistration");

    return 0;
}

static int test_file_handling(void) {
    data_file_t file = {0};
    
    TEST_ASSERT(dsc_open_file(&file, "nonexistent.txt") == DSC_ERROR, "Opening nonexistent file should fail");
    TEST_ASSERT(dsc_get_error() != NULL, "Error message should be set");

    return 0;
}

int main(void) {
    printf("Starting tests...\n");

    // Initialize the system
    if (dsc_init() != DSC_SUCCESS) {
        fprintf(stderr, "Failed to initialize system\n");
        return 1;
    }

    // Run tests
    TEST_RUN(test_command_registration);
    TEST_RUN(test_file_handling);

    // Cleanup
    dsc_cleanup();

    printf("All tests passed!\n");
    return 0;
} 