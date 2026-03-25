#pragma once

// =============================================================================
// file_watcher.hpp — Lightweight directory change monitor
// =============================================================================
//
// Non-blocking, poll-based directory watcher for auto-refreshing file lists.
// Linux: inotify (zero-cost when idle — single read() returning EAGAIN).
// Other platforms: no-op stub (returns false from poll_changed).
//
// =============================================================================

#include <string>

#ifdef __linux__
#include <sys/inotify.h>
#include <unistd.h>
#endif

namespace file_watcher {

class FileWatcher {
public:
    FileWatcher() {
#ifdef __linux__
        inotify_fd_ = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
#endif
    }

    ~FileWatcher() {
        unwatch();
#ifdef __linux__
        if (inotify_fd_ >= 0) ::close(inotify_fd_);
#endif
    }

    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;

    /// Watch a directory for content changes. Replaces any prior watch.
    bool watch(const std::string& path) {
        unwatch();
        watched_path_ = path;
#ifdef __linux__
        if (inotify_fd_ < 0) return false;
        watch_fd_ = inotify_add_watch(inotify_fd_, path.c_str(),
            IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);
        return watch_fd_ >= 0;
#else
        return false;
#endif
    }

    /// Stop watching.
    void unwatch() {
#ifdef __linux__
        if (inotify_fd_ >= 0 && watch_fd_ >= 0) {
            inotify_rm_watch(inotify_fd_, watch_fd_);
            watch_fd_ = -1;
        }
#endif
        watched_path_.clear();
    }

    /// Non-blocking check. Returns true if directory changed since last call.
    bool poll_changed() {
#ifdef __linux__
        if (inotify_fd_ < 0 || watch_fd_ < 0) return false;

        // Drain all pending events
        alignas(struct inotify_event) char buf[4096];
        bool changed = false;
        for (;;) {
            ssize_t n = ::read(inotify_fd_, buf, sizeof(buf));
            if (n <= 0) break;
            changed = true;
        }
        return changed;
#else
        return false;
#endif
    }

    bool is_watching() const {
#ifdef __linux__
        return watch_fd_ >= 0;
#else
        return false;
#endif
    }

    const std::string& watched_path() const { return watched_path_; }

private:
#ifdef __linux__
    int inotify_fd_ = -1;
    int watch_fd_ = -1;
#endif
    std::string watched_path_;
};

} // namespace file_watcher
