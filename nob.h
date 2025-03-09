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
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

typedef std::vector<const char*> cmd_list;
typedef std::vector<const char*> source_list;

#define shift_args(cnt, args) (assert(cnt > 0), (cnt)--, *(args)++)

#ifndef REBUILD_SELF
# define REBUILD_SELF(argc, argv) go_rebuild_self(argc, argv, __FILE__)
# define REBUILD_SELF_AND_WATCH(argc, argv, ...) go_rebuild_self(argc, argv, __FILE__, __VA_ARGS__)
#endif

#ifndef COMMAND
# define COMMAND(...) { cmd_list cmd = { __VA_ARGS__ }; run_command_sync(&cmd); }
#endif

// currently only supporting 
#ifndef REBUILD_SELF_PARAMS
# ifdef _MSVC_LANG
#  error Implementation for MSVC is missing as of now
# elif defined(__clang__)
#  define REBUILD_SELF_PARAMS(target, src) "clang++", "-Wall", "-Werror", "-Wpedantic", "-std=c++17", "-o", target, src
# elif defined(__GXX_ABI_VERSION)
#  define REBUILD_SELF_PARAMS(target, src) "g++", "-Wall", "-Werror", "-Wpedantic", "-std=c++17", "-o", target, src
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
    source_list sources = { args... };
    sources.push_back(source_file);

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
    if(run_command_sync(&cmd) < 0)
    {
        _log(log_level::error, "Error while trying to rebuild self");
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
    if(rename(file, new_file) < 0)
    {
        _log(log_level::error, "Failed renaming file: %s", strerror(errno));
        return -1;
    }

    return 1;
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
