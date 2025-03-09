#include "nob.h"
#include <iostream>

int main (int argc, char *argv[])
{
    REBUILD_SELF(argc, argv);

    COMMAND("g++", "-Wall", "-Werror", "-Wpedantic", "-o", "./example/main", "./example/main.cpp");
    COMMAND("./example/main");
    return 0;
}
