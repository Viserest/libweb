#define NOB_IMPLEMENTATION
#include "nob.h"

#include "src/types.h"

#define SRC_FOLDER "src/"
#define BUILD_FOLDER "build/"

typedef struct {
    const i8* name;
    const i8* signature;
    const i8* description;
} Command;

typedef struct {
    Command *items;
    usize count;
    usize capacity;

    bool picked;
    const i8* picked_name;
    const i8* picked_at_file;
    i32 picked_at_line;
} Commands;

void commands_reset(Commands *commands) {
    commands->count = 0;
    commands->picked = false;
}

#define command(arg, commands, name, signature, description) command_loc(__FILE__, __LINE__, (arg), (commands), (name), (signature), (description))
bool command_loc(const i8* file, i32 line, const i8* arg, Commands *commands, const i8* name, const i8* signature, const i8* description)
{
    if (commands->picked) {
        fprintf(stderr, "%s:%d: ASSERTION FAILED: the branch for command `%s` fell through.\n", commands->picked_at_file, commands->picked_at_line, commands->picked_name);
        fprintf(stderr, "%s:%d: NOTE: the execution proceeded to here, but the command was already picked.\n", file, line);
        abort();
    }
    Command command = {
        .name = name,
        .signature = signature,
        .description = description,
    };
    da_append(commands, command);
    commands->picked_name    = name;
    commands->picked_at_line = line;
    commands->picked_at_file = file;
    commands->picked         = (strcmp(arg, name) == 0);
    return commands->picked;
}

void print_available_commands(Commands commands)
{
    usize max_name_width = 0;
    usize max_sign_width = 0;
    da_foreach(Command, command, &commands) {
        usize name_width = strlen(command->name);
        usize sign_width = strlen(command->signature);
        if (name_width > max_name_width) max_name_width = name_width;
        if (sign_width > max_sign_width) max_sign_width = sign_width;
    }
    nob_log(INFO, "Available commands:");
    da_foreach(Command, command, &commands) {
        nob_log(INFO, "    %-*s %-*s - %s", (int)max_name_width, command->name, (int)max_sign_width, command->signature, command->description);
    }
}

bool build_and_run_test(const i8* test_name) {
    bool result = true;

    // Make binary
    Cmd cmd = {0};
    nob_cc(&cmd);
    nob_cc_flags(&cmd);
    nob_cc_inputs(&cmd, nob_temp_sprintf(SRC_FOLDER"%s/test.c", test_name));
    nob_cc_output(&cmd, nob_temp_sprintf(BUILD_FOLDER"%s", test_name));
    if (!nob_cmd_run(&cmd)) {
        nob_log(NOB_ERROR, "Compiling test %s", test_name);
        nob_return_defer(false);
    }

    // Run binary
    cmd_append(&cmd, temp_sprintf(BUILD_FOLDER"%s", test_name));
    if (!nob_cmd_run(&cmd)) nob_return_defer(false);

    nob_log(NOB_INFO, "--- %s finished ---", test_name);

defer:
    cmd_free(cmd);
    return result;
}

i32 main(i32 argc, i8** argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    const i8* program_name = shift(argv, argc);
    // Set default command name
    const i8* command_name = "build";
    if (argc > 0) command_name = shift(argv, argc);

    Commands commands = {0};
    commands_reset(&commands);

    if (command(command_name, &commands, "build", "", "Build the project")) {
        if (!nob_mkdir_if_not_exists(BUILD_FOLDER)) return 1;

        Nob_Cmd cmd = {0};
        nob_cc(&cmd);
        nob_cc_flags(&cmd);
        nob_cc_inputs(&cmd, SRC_FOLDER"http/http_parser.c", SRC_FOLDER"json/json_parser.c", SRC_FOLDER"main.c");
        nob_cc_output(&cmd, BUILD_FOLDER"main");
        if (!nob_cmd_run(&cmd)) return 1;
        return 0;
    }

    if (command(command_name, &commands, "test", "[test_names...]", "Run the tests checking their expected output")) {
        if (!nob_mkdir_if_not_exists(BUILD_FOLDER)) return 1;
        if (!nob_mkdir_if_not_exists(BUILD_FOLDER"tests/")) return 1;

        const i8* tests[] = {
            "http",
            "json",
        };
        usize tests_size = NOB_ARRAY_LEN(tests);

        usize failed_count = 0;
        if (argc <= 0) {
            for (size_t i = 0; i < tests_size; ++i) {
                size_t mark = temp_save();
                if (!build_and_run_test(tests[i])) failed_count += 1;
                temp_rewind(mark);
            }
        } else {
            while (argc > 0) {
                size_t mark = temp_save();
                const char *test_name = shift(argv, argc);
                if (!build_and_run_test(test_name)) failed_count += 1;
                temp_rewind(mark);
            }
        }
        nob_log(NOB_ERROR, "Finished running tests.");

        return 0;
    }

    if (command(command_name, &commands, "help", "", "Print this help message")) {
        print_available_commands(commands);
        return 0;
    }

    print_available_commands(commands);
    nob_log(NOB_ERROR, "Unknown command %s", command_name);
    return 1;
}
