TARGET_EXEC ?= ds

BUILD_DIR ?= ./build
SRC_DIRS ?= ./src
INCLUDE_DIRS ?= ./include
TEST_DIRS ?= ./tests

# Source files
SRCS := $(shell find $(SRC_DIRS) -name *.c)
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

# Test files
TEST_SRCS := $(shell find $(TEST_DIRS) -name *.c)
TEST_OBJS := $(TEST_SRCS:%=$(BUILD_DIR)/%.o)
TEST_DEPS := $(TEST_OBJS:.o=.d)

# Include directories
INC_DIRS := $(shell find $(SRC_DIRS) -type d) $(INCLUDE_DIRS)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

# Compiler flags
CFLAGS := -Wall -Wstrict-prototypes -Wmissing-prototypes -Wshadow -Wconversion -g
CPPFLAGS ?= $(INC_FLAGS) -MMD -MP

# Debug flags (uncomment to enable)
#CFLAGS += -DDEBUG=1
#CXXFLAGS += -g -DDEBUG=1

# Sanitizer flags (uncomment to enable)
#CFLAGS += -fsanitize=address -fno-omit-frame-pointer
#LDFLAGS += -fsanitize=address

# Main target
$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Test target
$(BUILD_DIR)/test_runner: $(TEST_OBJS)
	$(CC) $(TEST_OBJS) -o $@ $(LDFLAGS)

# Object files
$(BUILD_DIR)/%.c.o: %.c
	$(MKDIR_P) $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Test object files
$(BUILD_DIR)/$(TEST_DIRS)/%.c.o: $(TEST_DIRS)/%.c
	$(MKDIR_P) $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Phony targets
.PHONY: all clean test

all: $(BUILD_DIR)/$(TARGET_EXEC)

test: $(BUILD_DIR)/test_runner
	$(BUILD_DIR)/test_runner

clean:
	$(RM) -r $(BUILD_DIR)

# Include dependencies
-include $(DEPS)
-include $(TEST_DEPS)

MKDIR_P ?= mkdir -p
