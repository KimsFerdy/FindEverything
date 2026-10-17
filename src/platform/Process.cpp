#include "platform/Process.h"

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>

namespace fe::posix {

namespace {

// build argv before fork(). after fork in a threaded process the child can only call
// async-signal-safe stuff (no malloc) until exec. claude helped me get this part right
std::vector<char*> toArgv(const std::vector<std::string>& args) {
    std::vector<char*> out;
    for (const std::string& a : args) out.push_back(const_cast<char*>(a.c_str()));
    out.push_back(nullptr);
    return out;
}

// child side: stdio to /dev/null (except captured stdout), close every other fd we inherited
void sanitizeChildFds(bool keepStdout) {
    int devnull = open("/dev/null", O_RDWR);
    if (devnull >= 0) {
        dup2(devnull, STDIN_FILENO);
        if (!keepStdout) dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
    }
    close_range(3, ~0U, 0);
}

int waitExitCode(pid_t pid) {
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) return -1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

}  // namespace

bool spawnDetached(const std::vector<std::string>& args) {
    if (args.empty()) return false;
    std::vector<char*> argv = toArgv(args);

    // double fork: middle child exits right away, grandchild gets adopted by init.
    // no zombies left behind and we never have to waitpid() on it
    pid_t child = fork();
    if (child < 0) return false;
    if (child == 0) {
        setsid();
        pid_t grandchild = fork();
        if (grandchild != 0) _exit(grandchild < 0 ? 1 : 0);
        sanitizeChildFds(false);
        execvp(argv[0], argv.data());
        _exit(127);
    }
    return waitExitCode(child) == 0;
}

int runCapture(const std::vector<std::string>& args, std::string* output) {
    if (args.empty()) return -1;
    std::vector<char*> argv = toArgv(args);

    int pipeFds[2];
    if (pipe2(pipeFds, O_CLOEXEC) != 0) return -1;

    pid_t child = fork();
    if (child < 0) {
        close(pipeFds[0]);
        close(pipeFds[1]);
        return -1;
    }
    if (child == 0) {
        dup2(pipeFds[1], STDOUT_FILENO);  // dup2 clears CLOEXEC on the new fd
        sanitizeChildFds(true);
        execvp(argv[0], argv.data());
        _exit(127);
    }

    close(pipeFds[1]);  // or read() never hits EOF
    char buf[4096];
    ssize_t n;
    while ((n = read(pipeFds[0], buf, sizeof buf)) != 0) {
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (output) output->append(buf, static_cast<size_t>(n));
    }
    close(pipeFds[0]);
    return waitExitCode(child);
}

}  // namespace fe::posix
