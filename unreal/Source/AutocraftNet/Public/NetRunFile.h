// The world server's run file (S2, docs/networking.md "The world runs on"):
// a server that outlives the game (`autocraft-server --detached`) writes
// one beside the save it serves (`first.sqlite`: `first.run`) with its pid,
// port, protocol, build and one-time token, and removes it when it stops.
// The game reads it on start and reattaches to a live server of its own
// build, or asks an older one to save and stop and starts its own;
// `autocraft-server --stop <save>` and the home screen's STOP stop it. A
// file whose process is gone (a crash, a reboot) is stale: removed when
// checked. POSIX (the Mac and Linux); elsewhere nothing is ever live.
#pragma once

#include "AutocraftNetApi.h"

#include <cstdint>
#include <optional>
#include <string>

namespace ac::net {

struct RunFile {
    int pid = 0;
    uint16_t port = 0;
    /// Its protocol version (`kProtocolVersion` when it started).
    uint16_t protocol = 0;
    /// Its program's build (`programBuild`).
    std::string build;
    /// What a client answers with (the one-time token it was handed).
    std::string token;
    /// When it started, Unix seconds.
    int64_t since = 0;
    /// The save it serves.
    std::string save;

    bool operator==(const RunFile&) const = default;
};

/// The run file of the save `save`: its name with `.run` for `.sqlite`
/// (`mmo/first.sqlite`: `mmo/first.run`), else `.run` added.
AUTOCRAFTNET_API std::string runFilePath(const std::string& save);
/// `key value` lines; unknown keys are skipped when read.
AUTOCRAFTNET_API std::string encodeRunFile(const RunFile& file);
/// Unset when a needed line (pid, port, protocol, build, token) is missing.
AUTOCRAFTNET_API std::optional<RunFile> decodeRunFile(const std::string& text);
/// Written aside and moved in (no reader sees half of it), readable only by
/// its owner (it holds the token). False, with `why`, when it could not be.
AUTOCRAFTNET_API bool writeRunFile(const std::string& path, const RunFile& file, std::string* why = nullptr);
AUTOCRAFTNET_API std::optional<RunFile> readRunFile(const std::string& path);
/// Removes `path` only when it still names `pid` (a newer server's file
/// stays).
AUTOCRAFTNET_API void removeRunFile(const std::string& path, int pid);

/// A program's build: its size and modification time (a rebuild or a new
/// package changes it). Empty when it is not there.
AUTOCRAFTNET_API std::string programBuild(const std::string& program);
/// This process's own program (its path; empty when not known).
AUTOCRAFTNET_API std::string ownProgram();
/// `pid` is a running `autocraft-server` (not gone, not a zombie, not a
/// pid reused by another program).
AUTOCRAFTNET_API bool serverRunning(int pid);

enum class RunState { none, live, stale };
struct RunCheck {
    RunState state = RunState::none;
    /// Live: what it says.
    std::optional<RunFile> file;
};
/// The run file at `path`: none there, a live server, or stale (unreadable,
/// or its server gone), which is removed.
AUTOCRAFTNET_API RunCheck checkRunFile(const std::string& path);
/// The running server is the one `program` would be: the same protocol as
/// this library's and the same build.
AUTOCRAFTNET_API bool runFileCurrent(const RunFile& file, const std::string& program);
/// Asks it to stop (SIGTERM: it saves, then ends) and waits up to `seconds`
/// for it to be gone. True when gone (or it was already).
AUTOCRAFTNET_API bool stopServer(const RunFile& file, double seconds);

/// What the game does on start with the run file beside its save: never two
/// servers on one save.
struct Reattach {
    enum class Do {
        /// None runs (or a stale file, removed; or an older one stopped):
        /// start one.
        start,
        /// One of `program`'s build runs: connect to `server`.
        reattach,
        /// An older one did not stop in time: `why`.
        failed,
    };
    Do what = Do::start;
    std::optional<RunFile> server;
    /// An older one (another protocol or build) asked to save and stop.
    std::optional<RunFile> replaced;
    bool stale = false;
    std::string why;
};
AUTOCRAFTNET_API Reattach reattachOrReplace(const std::string& save, const std::string& program, double stopSeconds = 10);

} // namespace ac::net
