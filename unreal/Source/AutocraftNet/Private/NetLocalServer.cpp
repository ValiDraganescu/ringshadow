#include "NetLocalServer.h"

#if defined(_WIN32)

namespace ac::net {

LocalServer::~LocalServer() = default;

bool LocalServer::start(const std::string&, const std::vector<std::string>&, const std::string&, bool) {
    error_ = "the local server is not started on this platform yet";
    return false;
}

LocalServer::State LocalServer::poll() { return state_; }
bool LocalServer::stop(double) { return true; }
void LocalServer::release() {}
void LocalServer::exited(int) {}
void LocalServer::readOut() {}
void LocalServer::closePipes() {}

} // namespace ac::net

#else

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char** environ;

namespace ac::net {

namespace {

void pause10ms() {
    timespec t{0, 10'000'000};
    nanosleep(&t, nullptr);
}

} // namespace

LocalServer::~LocalServer() { stop(3.0); }

bool LocalServer::start(const std::string& program, const std::vector<std::string>& args, const std::string& token, bool detached) {
    stop(0.5);
    state_ = State::idle;
    error_.clear();
    port_ = 0;
    exitCode_ = -1;
    line_.clear();
    if (access(program.c_str(), X_OK) != 0) {
        error_ = "no server program at " + program;
        return false;
    }
    int toChild[2], fromChild[2];
    if (pipe(toChild) != 0) {
        error_ = std::string("pipe: ") + std::strerror(errno);
        return false;
    }
    if (pipe(fromChild) != 0) {
        error_ = std::string("pipe: ") + std::strerror(errno);
        close(toChild[0]);
        close(toChild[1]);
        return false;
    }
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, toChild[0], 0);
    posix_spawn_file_actions_adddup2(&actions, fromChild[1], 1);
    posix_spawn_file_actions_addopen(&actions, 2, "/dev/null", O_WRONLY, 0);
    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);
    short flags = 0;
#ifdef POSIX_SPAWN_CLOEXEC_DEFAULT
    // Only the three it is given: none of the game's own files and sockets.
    flags |= POSIX_SPAWN_CLOEXEC_DEFAULT;
#else
    posix_spawn_file_actions_addclose(&actions, toChild[1]);
    posix_spawn_file_actions_addclose(&actions, fromChild[0]);
#endif
    // Its own process group: a Ctrl-C meant for the game's terminal is not
    // its. Detached, a session of its own: nothing that ends the game's
    // (a quit, a crash, a closed terminal) reaches it.
#ifdef POSIX_SPAWN_SETSID
    if (detached) flags |= POSIX_SPAWN_SETSID;
    else
#endif
    {
        (void)detached;
        flags |= POSIX_SPAWN_SETPGROUP;
        posix_spawnattr_setpgroup(&attr, 0);
    }
    posix_spawnattr_setflags(&attr, flags);
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(program.c_str()));
    for (const std::string& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);
    pid_t pid = -1;
    const int rc = posix_spawn(&pid, program.c_str(), &actions, &attr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attr);
    close(toChild[0]);
    close(fromChild[1]);
    if (rc != 0) {
        close(toChild[1]);
        close(fromChild[0]);
        error_ = std::string("could not start ") + program + ": " + std::strerror(rc);
        return false;
    }
    pid_ = pid;
    out_ = fromChild[0];
    fcntl(out_, F_SETFL, fcntl(out_, F_GETFL) | O_NONBLOCK);
    fcntl(out_, F_SETFD, FD_CLOEXEC);
    // The token, then its standard input closes: a child that died already
    // must not take the game with it (SIGPIPE).
#ifdef F_SETNOSIGPIPE
    fcntl(toChild[1], F_SETNOSIGPIPE, 1);
#endif
    const std::string line = token + "\n";
    const ssize_t wrote = write(toChild[1], line.data(), line.size());
    close(toChild[1]);
    state_ = State::starting;
    if (wrote != ssize_t(line.size())) {
        error_ = "could not hand the server its token";
        stop(1.0);
        return false;
    }
    return true;
}

void LocalServer::exited(int status) {
    exitCode_ = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
    if (error_.empty()) {
        error_ = WIFEXITED(status) ? "exited with code " + std::to_string(WEXITSTATUS(status))
                                   : "ended by signal " + std::to_string(WTERMSIG(status));
    }
    state_ = State::exited;
    pid_ = -1;
    closePipes();
}

void LocalServer::closePipes() {
    if (out_ >= 0) close(out_);
    out_ = -1;
}

void LocalServer::readOut() {
    // What it printed: the port line (or why it did not start), then nothing
    // it needs read, but the pipe is kept empty so it never blocks on it.
    char buffer[256];
    for (;;) {
        if (out_ < 0) break;
        const ssize_t n = read(out_, buffer, sizeof buffer);
        if (n <= 0) break;
        for (ssize_t i = 0; i < n; i++) {
            if (buffer[i] != '\n') {
                if (line_.size() < 200) line_.push_back(buffer[i]);
                continue;
            }
            unsigned port = 0;
            if (state_ == State::starting && std::sscanf(line_.c_str(), "PORT %u", &port) == 1 && port > 0 && port < 65536) {
                port_ = uint16_t(port);
                state_ = State::running;
            } else if (line_.rfind("FAILED ", 0) == 0) {
                error_ = line_.substr(7);
            }
            line_.clear();
        }
    }
}

LocalServer::State LocalServer::poll() {
    if (state_ != State::starting && state_ != State::running) return state_;
    readOut();
    int status = 0;
    const pid_t r = waitpid(pid_, &status, WNOHANG);
    if (r == pid_) {
        // What it printed before it ended (its reason) is in the pipe now.
        readOut();
        exited(status);
    }
    else if (r < 0 && errno == ECHILD && kill(pid_, 0) != 0) {
        // Reaped elsewhere (SIGCHLD ignored): gone, how is not known.
        error_ = error_.empty() ? "the server is gone" : error_;
        state_ = State::exited;
        pid_ = -1;
        closePipes();
    }
    return state_;
}

void LocalServer::release() {
    closePipes();
    pid_ = -1;
    state_ = State::idle;
}

bool LocalServer::stop(double waitSeconds) {
    if (pid_ <= 0) {
        closePipes();
        return true;
    }
    kill(pid_, SIGTERM);
    bool clean = false;
    for (double waited = 0; waited < waitSeconds; waited += 0.01) {
        int status = 0;
        const pid_t r = waitpid(pid_, &status, WNOHANG);
        if (r == pid_) {
            const std::string kept = error_;
            exited(status);
            error_ = kept.empty() ? "stopped" : kept;
            clean = exitCode_ == 0;
            break;
        }
        if (r < 0) {
            state_ = State::exited;
            pid_ = -1;
            clean = true;
            break;
        }
        pause10ms();
    }
    if (pid_ > 0) {
        kill(pid_, SIGKILL);
        int status = 0;
        waitpid(pid_, &status, 0);
        exited(status);
    }
    closePipes();
    return clean;
}

} // namespace ac::net

#endif
