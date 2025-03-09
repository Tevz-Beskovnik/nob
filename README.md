# nob

Nob is a inspiration base on the [nob.h](https://github.com/tsoding/nob.h.git) written by Tsoding. I take 0 credit for most of the code written here I just wanted to see I can recreate the buildsystem and understand how it works but here using c++ instead of c. 

## What is it

Essentialy it is a no-build system concept where the only thing you need to build your project is `nob.h`, a c++ compiler and knowlage of the c++ language.

However nob is mostlikely not suitable for you if you are relying on external dependencies, via CMake. It's mostly a helper tool to run commands in the shell for you.

## How to use nob

Create a file named `nob.cpp`, thats where all your compile commands will go, in that file you want to include `nob.h` and the code to automaticly rebuild if the build file changes:

```cpp
#include "nob.h"

int main (int argc, char *argv[])
{
    REBUILD_SELF(argc, argv); // this command checks for changes for itself and utomaticly rebuilds if needed
    // alternitavely if you have external dependecies to your build file you add them to the watch list too
    // REBUILD_SELF_AND_WATCH(argc, argv, "./path/to/dependency", ...);

    return 0;
}

```

To call a command from the main function just use the `COMMAND()` macro, to run any shell command, space separated commands/arguemts should be separated by a comma:

```cpp
COMMAND("echo", "foo");

COMMAND("g++", "-Wall", "-Werror", "-o", "out", "foo.cpp");

//... and so on
```

**More features are to come**
