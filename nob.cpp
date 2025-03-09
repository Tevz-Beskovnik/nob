#include "nob.h"
#include <iostream>

int main (int argc, char *argv[])
{
    REBUILD_SELF(argc, argv);

    cmd_list command = { "echo", "test" };
    run_command_sync(&command);
    cmd_list command2 = { "echo", "test2" };
    run_command_sync(&command2);
    //COMMAND("echo", "test2");
    return 0;
}
