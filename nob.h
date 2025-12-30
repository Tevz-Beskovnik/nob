/* 
 * nob is the concept of no-build where a developer should not need any other tools to quickly
 * build a project spanning multiple files other then the g++ compiler toolchain,
 * however this it is mostlikely not suitable if you depend on external libraries
 * most of the inspiration for this little project com from Tsodings nob.h implementation:
 * https://github.com/tsoding/nob.h.git
 */

#pragma once
#include <assert.h>
#include <cerrno>
#include <cstdarg>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>

typedef std::vector<const char*> cmd_list;
typedef std::vector<const char*> source_list;
typedef source_list directory_list;

#define shift_args(cnt, args) (assert(cnt > 0), (cnt)--, *(args)++)

#ifndef BUILD_WATCH_LIST
#define BUILD_WATCH_LIST
#endif

#ifndef BUILD_ADDITIONAL_FLAGS
#define BUILD_ADDITIONAL_FLAGS 
#endif

#ifndef BUILD_CUSTOM_COMMAND
# ifdef _MSVC_LANG
#  error Implementation for MSVC is missing as of now
# elif defined(__clang__)
#  define BUILD_CUSTOM_COMMAND "clang++"
# elif defined(__GXX_ABI_VERSION)
#  define BUILD_CUSTOM_COMMAND "g++"
# endif
#endif

#ifndef REBUILD_SELF
# define REBUILD_SELF(argc, argv) go_rebuild_self(argc, argv, __FILE__)
# define REBUILD_SELF_AND_WATCH(argc, argv, ...) go_rebuild_self(argc, argv, __FILE__, __VA_ARGS__)
#endif

#ifndef COMMAND_DEFS
#define COMMAND_DEFS
# define COMMAND(...) { cmd_list cmd = { __VA_ARGS__ }; run_command_sync(&cmd); }
# define COMMAND_ASYNC(...) { cmd_list cmd = { __VA_ARGS__ }; run_command_async(&cmd); 0
#endif

#ifndef DIR_COMMAND_DEFS
#define DIR_COMMAND_DEFS
# ifdef _WIN32
# error("Not implemented for windows")
# elif defined (__unix__) || defined (__APPLE__)
// creation cmds
#  define CREATE_DIRS(...) COMMAND("mkdir", __VA_ARGS__)
#  define CREATE_DIRS_ASYNC(...) COMMAND_ASYNC("mkdir", __VA_ARGS__)
// deletion cmds
#  define REMOVE_DIR_ASYNC(dir) COMMAND_ASYNC("rm", "-f", dir)
#  define REMOVE_DIRS_ASYNC(...) COMMAND_ASYNC("rm", "-f", __VA_ARGS__)
#  define REMOVE_DIR_RECURSIVE(dir) COMMAND("rm", "-rf", dir)
#  define REMOVE_DIR_RECURSIVE_ASYNC(dir) COMMAND_ASYNC("rm", "-rf", dir)
#  define REMOVE_DIRS_RECURSIVE(...) COMMAND("rm", "-rf", __VA_ARGS__)
#  define REMOVE_DIRS_RECURSIVE_ASYNC(...) COMMAND_ASYNC("rm", "-rf", __VA_ARGS__)
# else
#  error("Unsupported platform")
# endif
#endif

#ifndef FILE_COMMAND_DEFS
#define FILE_COMMAND_DEFS
# ifdef _WIN32
# elif defined (__unix__) || defined (__APPLE__)
// file creation
# else
#  error("Unsupported platform")
# endif
#endif

// currently only supporting unix and macos
#ifndef REBUILD_SELF_PARAMS
# ifdef _MSVC_LANG
#  error Implementation for MSVC is missing as of now
# elif defined(__clang__)
#  define REBUILD_SELF_PARAMS(target, src) BUILD_CUSTOM_COMMAND, "-Wall", "-Wpedantic", "-std=c++20",  "-o", target, src
# elif defined(__GXX_ABI_VERSION)
#  define REBUILD_SELF_PARAMS(target, src) BUILD_CUSTOM_COMMAND, "-Wall", "-Wpedantic", "-std=c++20", "-o", target, src
# endif
#endif

enum class log_level
{
    info = 0,
    warn = 1,
    error = 2
};

void _log(log_level lvl, const char* output, ...);
void output_command(const cmd_list *command);
int run_command_sync(cmd_list *command);
int run_command_async(cmd_list *command);
int should_rebuild_self(const char *binary, source_list sources);
int rename_file(const char *file, const char *new_file);
int wait_for_command(pid_t pid);
void run_command_sync_no_fork(cmd_list *command);

template<typename... T>
inline void go_rebuild_self(int argc, char **argv, const char* source_file, T... args)
{
    const char *binary_path = shift_args(argc, argv);
    source_list sources = { BUILD_WATCH_LIST };
    source_list sources_without_main = { args... };
    sources.push_back(source_file);
    sources.insert(
            sources.end(),
            sources_without_main.begin(),
            sources_without_main.end()
        );

    int rebuild_needed = should_rebuild_self(binary_path, sources);
    if(rebuild_needed < 0)
    {
        _log(log_level::error, "Something went wrong while determening if rebuild is needed");
        exit(1);
    }
    if(rebuild_needed == 0) return;

    std::string old_binary_path = binary_path;
    old_binary_path += ".old";
    if(rename_file(binary_path, old_binary_path.c_str()) < 0)
    {
        _log(log_level::error, "Error while trying to rename file to .old");
        exit(1);
    }

    cmd_list cmd = { REBUILD_SELF_PARAMS(binary_path, source_file) };
    cmd.insert(cmd.end(), { BUILD_ADDITIONAL_FLAGS });
    cmd.insert(cmd.end(), { args... });
    if(run_command_sync(&cmd) < 0)
    {
        _log(log_level::error, "Error while trying to rebuild self");

        if(rename_file(old_binary_path.c_str(), binary_path) < 0)
        {
            _log(log_level::error, "Error while trying to rename file to .old");
            exit(1);
        }

        exit(1);
    }

    _log(log_level::info, "Trying to delete %s executable", old_binary_path.c_str());
    if(remove(old_binary_path.c_str()) < 0) 
    {
        _log(log_level::warn, "Failed to delete .old binary file");
    }

    cmd_list exec_bin = { binary_path };
    for(int i = 0; i < argc; i++) exec_bin.push_back(argv[i]);
    run_command_sync_no_fork(&exec_bin);
}

inline void output_command(const cmd_list* command)
{
    std::string output = "";
    for(const auto segment : *command)
    {
        output += segment;
        output += ' ';
    }

    _log(log_level::info, "CMD: %s", output.c_str());
}

inline int run_command_async(cmd_list* command)
{
    assert(command->size() > 0);

    pid_t cpid = fork();
    if(cpid < 0)
    {
        _log(log_level::error, "Error while trying to fork process: %s", cpid);
        exit(1);
    }

    if(cpid == 0) // if cpid 0 we're in child process
    {
        cmd_list command_null { *command };
        command_null.push_back(NULL);

        output_command(command);

        if(execvp((*command)[0], (char* const*)command_null.data()) < 0) // this will exit out with the signal the calling command returns
        {
            _log(log_level::error, "Error while calling command: %s\n", strerror(errno));
            exit(1);
        }
    }

    return cpid;
}

inline int run_command_sync(cmd_list* command)
{
    pid_t pid = run_command_async(command);
    return wait_for_command(pid);
}

inline void run_command_sync_no_fork(cmd_list *command)
{
    assert(command->size() > 0);

    cmd_list command_null { *command };
    command_null.push_back(NULL);

    output_command(command);

    if(execvp((*command)[0], (char* const*)command_null.data()) < 0) // this will exit out with the signal the calling command returns
    {
        _log(log_level::error, "Error while calling command: %s\n", strerror(errno));
        exit(1);
    }
}

inline int wait_for_command(pid_t pid) 
{
    while(1) 
    {
        int status;
        if(waitpid(pid, &status, 0) < 0)
        {
            _log(log_level::error, "Error trying to wait for command: %s", strerror(errno));
            return -1;
        }

        if(WIFEXITED(status))
        {
            int exit_status = WEXITSTATUS(status);
            if(exit_status != 0)
            {
                _log(log_level::error, "Exit signal from command was not 0");
                return -1;
            }
            return 0;
        };
        if(WIFSIGNALED(status) || WIFSTOPPED(status) || WCOREDUMP(status))
        {
            _log(log_level::error, "Error while executing command");
            return -1;
        };
    }

    return 1;
}

inline int should_rebuild_self(const char *binary, source_list sources)
{
    struct stat file_stat {0};

    if(stat(binary, &file_stat) < 0)
    {
        if(errno == ENOENT) return 1; // nob binary should get rebuilt if it doesn't exist
        _log(log_level::error, "Failed getting binary stat: %s", strerror(errno));
    }

    int binary_file_time = file_stat.st_mtime;

    for(const auto& file : sources)
    {
        if(stat(file, &file_stat) < 0)
        {
            if(errno == ENOENT) continue;
            _log(log_level::error, "Failed getting stat for %s error: %s", file, strerror(errno));
            return -1;
        }

        if(file_stat.st_mtime > binary_file_time) return true;
    }

    return false;
}

inline int rename_file(const char *file, const char *new_file)
{
    _log(log_level::info, "Renaming %s to %s", file, new_file);

    if(rename(file, new_file) < 0)
    {
        _log(log_level::error, "Failed renaming file: %s", strerror(errno));
        return -1;
    }

    return 1;
}

inline void create_directory(const char *path_to_dir)
{
    _log(log_level::info, "Creating directory %s", path_to_dir);

    auto out = mkdir(path_to_dir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);

    if(out < 0)
    {
        if(errno == EEXIST) return;

        _log(log_level::error, "Failed creating directory: %s", strerror(errno));
        exit(0);
    }
}

template<typename ...T>
inline void create_directories(T... args)
{
    directory_list dirs = { args... };

    for(const auto& dir : dirs)
    {
        create_directory(dir);
    }
}

inline void remove_directory(const char *path_to_dir)
{
    _log(log_level::info, "Removing directory %s", path_to_dir);

    auto out = rmdir(path_to_dir);

    if(out < 0)
    {
        _log(log_level::error, "Failed removing directory: %s", strerror(errno));
        exit(0);
    }
}

template<typename ...T>
inline void remove_directories(T... args)
{
    directory_list dirs = { args... };

    for(const auto& dir : dirs)
    {
        remove_directory(dir);
    }
}

inline void remove_file(const char *file)
{
    _log(log_level::info, "Removing file %s", file);

    auto out = unlink(file);

    if(out < 0)
    {
        _log(log_level::error, "Failed to remove file: %s", strerror(errno));
        exit(0);
    }
}

template<typename... T>
inline void remove_files(T... args)
{
    source_list files = { args... };

    for(const auto& file : files)
    {
        remove_file(file);
    }
}

inline bool file_exists(const char *file)
{
    struct stat buffer;
    return (stat (file, &buffer) == 0);
}

inline void _log(log_level lvl, const char* fmt, ...)
{
    switch(lvl)
    {
        case log_level::info:
        {
            std::cerr << "[INFO] ";
            break;
        }
        case log_level::warn:
        {
            std::cerr << "[WARN] ";
            break;
        }
        case log_level::error:
        {
            std::cerr << "[ERROR] ";
            break;
        }
    }

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    printf("\n");
}
