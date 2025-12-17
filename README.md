# nob

Nob is a inspiration base on the [nob.h](https://github.com/tsoding/nob.h.git) written by Tsoding. I take 0 credit for most of the code written here I just wanted to see I can recreate the buildsystem and understand how it works but here using c++ instead of c. 

## What are the use cases?

Nob is a small single header library, with just enough tooling, to quickly iterate on small projects like commandline tools.

What does that look like? Add in the header, use the macro, build the executable once, **and off you go!**

**No need to recompile!** When writting new code, running the executable will automaticlay reompile it and run the new version.

**Well what if I add new source file?** No problem, add them to the list of files in the `REBUILD_SELF_AND_WATCH` macro, running the executable again will automaticaly build with the new files. This will work almost always, except if the file path is messedup, then the executable will need to be hand recompiled.

**What about testing?** Just create a new main file, create your test cases and call your program from within it via nobs `COMMAND` macro. With it you also get the benefit of automatical recompilation. You can also take advantage of nobs logging functions to print out success or failure of different tests (tho the API is currently not exposed, only via `_log` function)

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

Aditionaly you can specify a list of dependencies that should be in the watch list via a macro:

```cpp
#define BUILD_WATCH_LIST "./path/to/file", "../path/to/file2"
```

If you would like to add more build flags to the defualt build command use the `BUILD_ADDITIONAL_FLAGS` macro:

```cpp
#define BUILD_ADDITIONAL_FLAGS "-lpython3.8"
```

