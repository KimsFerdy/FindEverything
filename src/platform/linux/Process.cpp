#include "platform/linux/Process.h"

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>

namespace fe::posix {

namespace {

// exec wants a NULL-terminated char* array. Built *before* fork(), because after
// fork() in a multi-threaded program the child may only call async-signal-safe
// functions (no malloc!) until it execs.
std::vector<char*> toArgv(const std::vector<std::string>& args) {
    std::vector<char*> out;
    for (const std::string& a : args) out.push_back(const_cast<char*>(a.c_str()));
    out.push_back(nullptr);
    return out;
}

// In the child: point stdin/stderr (and stdout unless captured) at /dev/null and
// close every other inherited fd (our X11 socket, index files...).
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

    // Double fork: the middle child exits at once, so the grandchild is adopted
    // by init (PID 1) which reaps it. Otherwise every opened file would leave a
    // "zombie" process behind until we called waitpid() on it.
    pid_t child = fork();
    if (child < 0) return false;
    if (child == 0) {
        setsid();  // own session: closing our terminal won't kill it
        pid_t grandchild = fork();
        if (grandchild != 0) _exit(grandchild < 0 ? 1 : 0);
        sanitizeChildFds(false);
        execvp(argv[0], argv.data());
        _exit(127);  // only reached if exec failed
    }
    return waitExitCode(child) == 0;
}

int runCapture(const std::vector<std::string>& args, std::string* output) {
    if (args.empty()) return -1;
    std::vector<char*> argv = toArgv(args);

    int pipeFds[2];  // [0] = read end, [1] = write end
    if (pipe2(pipeFds, O_CLOEXEC) != 0) return -1;

    pid_t child = fork();
    if (child < 0) {
        close(pipeFds[0]);
        close(pipeFds[1]);
        return -1;
    }
    if (child == 0) {
        dup2(pipeFds[1], STDOUT_FILENO);  // dup2'd fds don't inherit O_CLOEXEC
        sanitizeChildFds(true);
        execvp(argv[0], argv.data());
        _exit(127);
    }

    close(pipeFds[1]);  // otherwise read() never sees EOF
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
