#ifndef DSC_COMMAND_H
#define DSC_COMMAND_H

#include "dsc.h"

// Command registration
int command_register(const command_t *cmd);
int command_unregister(const char *name);
command_t *command_get(const char *name);
void command_list_all(void);

// Command execution
int command_execute(const char *name, int argc, char **argv, void *data);
int command_execute_with_fs(const char *name, int argc, char **argv, void *data);

// Command validation
bool command_validate(const command_t *cmd);
bool command_has_required_args(const command_t *cmd, int argc);

// Command help
void command_print_help(const char *name);
void command_print_usage(const char *name);

#endif // DSC_COMMAND_H 