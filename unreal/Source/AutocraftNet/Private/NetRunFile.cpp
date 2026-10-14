#include "NetRunFile.h"

#include "NetPacket.h"

#include <cstdio>
#include <cstdlib>

namespace ac::net {

std::string runFilePath(const std::string& save) {
    static const std::string ext = ".sqlite";
    if (save.size() > ext.size() && save.compare(save.size() - ext.size(), ext.size(), ext) == 0)
        return save.substr(0, save.size() - ext.size()) + ".run";
    return save + ".run";
}

std::string encodeRunFile(const RunFile& f) {
    std::string out;
    out += "pid " + std::to_string(f.pid) + "\n";
    out += "port " + std::to_string(f.port) + "\n";
    out += "protocol " + std::to_string(f.protocol) + "\n";
    out += "build " + f.build + "\n";
    out += "token " + f.token + "\n";
    out += "since " + std::to_string(f.since) + "\n";
    out += "save " + f.save + "\n";
    return out;
}

std::optional<RunFile> decodeRunFile(const std::string& text) {
    RunFile f;
    bool pid = false, port = false, protocol = false, build = false, token = false;
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        const std::string line = text.substr(at, end - at);
        at = end + 1;
        const size_t space = line.find(' ');
        if (space == std::string::npos) continue;
        const std::string key = line.substr(0, space), value = line.substr(space + 1);
        char* rest = nullptr;
        if (key == "pid") {
            const long v = std::strtol(value.c_str(), &rest, 10);
            pid = *rest == 0 && v > 0;
            f.pid = int(v);
        } else if (key == "port") {
            const long v = std::strtol(value.c_str(), &rest, 10);
            port = *rest == 0 && v > 0 && v < 65536;
            f.port = uint16_t(v);
        } else if (key == "protocol") {
            const long v = std::strtol(value.c_str(), &rest, 10);
            protocol = *rest == 0 && v > 0 && v < 65536;
            f.protocol = uint16_t(v);
        } else if (key == "build") {
            f.build = value;
            build = !value.empty();
        } else if (key == "token") {
            f.token = value;
            token = !value.empty();
        } else if (key == "since") {
            f.since = std::strtoll(value.c_str(), &rest, 10);
        } else if (key == "save") {
            f.save = value;
        }
    }
    if (!pid || !port || !protocol || !build || !token) return std::nullopt;
    return f;
}

bool runFileCurrent(const RunFile& file, const std::string& program) {
    return file.protocol == kProtocolVersion && !file.build.empty() && file.build == programBuild(program);
}

Reattach reattachOrReplace(const std::string& save, const std::string& program, double stopSeconds) {
    Reattach r;
    const RunCheck check = checkRunFile(runFilePath(save));
    r.stale = check.state == RunState::stale;
    if (check.state != RunState::live) return r;
    if (runFileCurrent(*check.file, program)) {
        r.what = Reattach::Do::reattach;
        r.server = check.file;
        return r;
    }
    r.replaced = check.file;
    if (!stopServer(*check.file, stopSeconds)) {
        r.what = Reattach::Do::failed;
        r.why = "the older world server (pid " + std::to_string(check.file->pid) + ") did not stop";
    }
    return r;
}

} // namespace ac::net

#if defined(_WIN32)

namespace ac::net {

bool writeRunFile(const std::string&, const RunFile&, std::string* why) {
    if (why) *why = "no run file on this platform yet";
    return false;
}
std::optional<RunFile> readRunFile(const std::string&) { return std::nullopt; }
void removeRunFile(const std::string&, int) {}
std::string programBuild(const std::string&) { return {}; }
std::string ownProgram() { return {}; }
bool serverRunning(int) { return false; }
RunCheck checkRunFile(const std::string&) { return {}; }
bool stopServer(const RunFile&, double) { return true; }

} // namespace ac::net

#else

#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <libproc.h>
#endif

namespace ac::net {

namespace {

std::string readAll(const std::string& path, bool* found) {
    std::string out;
    *found = false;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return out;
    *found = true;
    char buf[1024];
    for (size_t n; (n = std::fread(buf, 1, sizeof buf, f)) > 0;) out.append(buf, n);
    std::fclose(f);
    return out;
}

/// The program `pid` runs (empty: gone, a zombie, or not ours to see).
std::string programOf(int pid) {
#if defined(__APPLE__)
    char path[PROC_PIDPATHINFO_MAXSIZE];
    if (proc_pidpath(pid, path, sizeof path) <= 0) return {};
    return path;
#else
    char path[4096];
    const std::string link = "/proc/" + std::to_string(pid) + "/exe";
    const ssize_t n = readlink(link.c_str(), path, sizeof path - 1);
    if (n <= 0) return {};
    path[n] = 0;
    return path;
#endif
}

} // namespace

bool writeRunFile(const std::string& path, const RunFile& file, std::string* why) {
    const std::string aside = path + ".new";
    const int fd = open(aside.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
        if (why) *why = aside + ": " + std::strerror(errno);
        return false;
    }
    const std::string text = encodeRunFile(file);
    const bool wrote = write(fd, text.data(), text.size()) == ssize_t(text.size());
    close(fd);
    if (!wrote || std::rename(aside.c_str(), path.c_str()) != 0) {
        if (why) *why = path + ": " + std::strerror(errno);
        unlink(aside.c_str());
        return false;
    }
    return true;
}

std::optional<RunFile> readRunFile(const std::string& path) {
    bool found = false;
    const std::string text = readAll(path, &found);
    if (!found) return std::nullopt;
    return decodeRunFile(text);
}

void removeRunFile(const std::string& path, int pid) {
    const auto f = readRunFile(path);
    if (f && f->pid == pid) unlink(path.c_str());
}

std::string programBuild(const std::string& program) {
    struct stat st {};
    if (program.empty() || stat(program.c_str(), &st) != 0) return {};
#if defined(__APPLE__)
    const timespec m = st.st_mtimespec;
#else
    const timespec m = st.st_mtim;
#endif
    return std::to_string(int64_t(st.st_size)) + "-" + std::to_string(int64_t(m.tv_sec)) + "." + std::to_string(int64_t(m.tv_nsec));
}

std::string ownProgram() { return programOf(int(getpid())); }

bool serverRunning(int pid) {
    if (pid <= 0) return false;
    if (kill(pid_t(pid), 0) != 0 && errno == ESRCH) return false;
    const std::string program = programOf(pid);
    const size_t slash = program.rfind('/');
    return (slash == std::string::npos ? program : program.substr(slash + 1)) == "autocraft-server";
}

RunCheck checkRunFile(const std::string& path) {
    bool found = false;
    const std::string text = readAll(path, &found);
    if (!found) return {};
    const auto f = decodeRunFile(text);
    if (f && serverRunning(f->pid)) return RunCheck{RunState::live, f};
    unlink(path.c_str());
    return RunCheck{RunState::stale, std::nullopt};
}

bool stopServer(const RunFile& file, double seconds) {
    if (!serverRunning(file.pid)) return true;
    kill(pid_t(file.pid), SIGTERM);
    for (double waited = 0; waited < seconds; waited += 0.02) {
        if (!serverRunning(file.pid)) return true;
        timespec t{0, 20'000'000};
        nanosleep(&t, nullptr);
    }
    return !serverRunning(file.pid);
}

} // namespace ac::net

#endif
