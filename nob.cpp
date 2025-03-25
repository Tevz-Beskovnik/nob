#include "nob.h"
#include <iostream>
#include <string>
#include <sys/stat.h>

void all()
{
    create_directory("./build");

    COMMAND("g++", "-Wall", "-Werror", "-Wpedantic", "-o", "./build/main", "./example/main.cpp");
    COMMAND("./build/main");
}

int main (int argc, char *argv[])
{
    REBUILD_SELF(argc, argv);
  
    if(argc >= 2)
    {
        std::cout << *argv << std::endl;
        std::cout << "shifting args" << std::endl;
        shift_args(argc, argv);

        std::cout << "shifting args" << std::endl;

        if (std::string(*argv) == "clean")
        {
            remove_file("./build/main");
            remove_directory("./build");
        }
        else
            all();
    }
    else
        all();
    
    return 0;
}
