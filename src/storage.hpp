#pragma once

#include <openssl/evp.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace gdocsmith {

constexpr size_t DOCUMENT_FILE_BYTES_MAX = 16 * 1024 * 1024; // 16 MiB

/// Snapshot of a file's state when read at the start of an operation
struct FileInputSnapshot {
    dev_t device = 0;
    ino_t inode = 0;
    std::string hash; // SHA-256 hex
    std::string path; // canonical / realpath
    std::string text; // content as UTF-8 string
};

/// Result of writing an export directory
struct ExportFolderResult {
    std::string exportFilePath; // working copy (next.md)
    std::string baseFilePath;   // unedited copy (base.md)
    std::string nextFilePath;   // working copy (next.md)
    std::optional<std::string> commentsFilePath;
    std::optional<std::string> suggestionsFilePath;
    std::vector<std::string> warnings;
};

namespace detail {

inline std::mutex g_storage_mutex;
inline std::optional<std::string> g_workspace_root;

inline std::string generate_unique_suffix() {
    static std::atomic<uint64_t> counter{0};
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    pid_t pid = getpid();
    uint64_t c = ++counter;
    char buf[64];
    snprintf(buf, sizeof(buf), "%d-%lld-%llu", static_cast<int>(pid), static_cast<long long>(now), static_cast<unsigned long long>(c));
    return std::string(buf);
}

inline std::string sha256_hex(std::string_view data) {
    unsigned char hash[EVP_MAX_MD_SIZE]{};
    unsigned int length = 0;
    if (EVP_Digest(data.data(), data.size(), hash, &length, EVP_sha256(), nullptr) != 1
        || length != 32) {
        throw std::runtime_error("Failed to compute SHA-256");
    }
    constexpr char digits[] = "0123456789abcdef";
    std::string hex(length * 2, '\0');
    for (size_t i = 0; i < length; ++i) {
        hex[i * 2] = digits[hash[i] >> 4];
        hex[i * 2 + 1] = digits[hash[i] & 15];
    }
    return hex;
}

inline std::string iso8601_filename_timestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&tt, &tm);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H-%M-%SZ", &tm);
    return std::string(buf);
}

inline std::string read_file_string(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

inline std::optional<std::string> read_file_string_opt(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return std::nullopt;
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

inline void atomic_write(const std::string& path, std::string_view content, mode_t mode = 0600) {
    std::string tempPath = path + "." + generate_unique_suffix() + ".tmp";
    int fd = open(tempPath.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, mode);
    if (fd < 0) {
        throw std::runtime_error("Failed to open temporary file: " + tempPath + ": " + strerror(errno));
    }
    size_t totalWritten = 0;
    while (totalWritten < content.size()) {
        ssize_t written = write(fd, content.data() + totalWritten, content.size() - totalWritten);
        if (written < 0) {
            if (errno == EINTR) continue;
            int err = errno;
            close(fd);
            unlink(tempPath.c_str());
            throw std::runtime_error("Failed to write temporary file: " + tempPath + ": " + strerror(err));
        }
        totalWritten += static_cast<size_t>(written);
    }
    fsync(fd);
    close(fd);
    if (rename(tempPath.c_str(), path.c_str()) != 0) {
        int err = errno;
        unlink(tempPath.c_str());
        throw std::runtime_error("Failed to atomic rename to " + path + ": " + strerror(err));
    }
}

} // namespace detail

/// Sets or clears the workspace root directory for storage
inline void set_workspace_root(const std::optional<std::string>& root) {
    std::lock_guard<std::mutex> lock(detail::g_storage_mutex);
    if (root.has_value() && !root->empty()) {
        detail::g_workspace_root = std::filesystem::absolute(*root).lexically_normal().string();
    } else {
        detail::g_workspace_root = std::nullopt;
    }
}

/// Resolves the private work root: GDOCSMITH_WORK_DIR, then workspace/.gdocsmith, then /tmp/gdocsmith-<uid>
inline std::string artifact_root() {
    std::lock_guard<std::mutex> lock(detail::g_storage_mutex);

    // 1. GDOCSMITH_WORK_DIR override
    const char* env = getenv("GDOCSMITH_WORK_DIR");
    if (env && *env != '\0') {
        std::string p = std::filesystem::absolute(env).lexically_normal().string();
        std::error_code ec;
        std::filesystem::create_directories(p, ec);
        return p;
    }

    // 2. Workspace root if not root ("/") or HOME directory
    if (detail::g_workspace_root.has_value()) {
        std::string ws = *detail::g_workspace_root;
        std::string root_str = std::filesystem::path(ws).root_path().string();

        const char* home_env = getenv("HOME");
        std::string home_str;
        if (home_env && *home_env != '\0') {
            home_str = std::filesystem::absolute(home_env).lexically_normal().string();
        } else {
            struct passwd* pw = getpwuid(getuid());
            if (pw && pw->pw_dir) {
                home_str = std::filesystem::absolute(pw->pw_dir).lexically_normal().string();
            }
        }

        if (ws != root_str && (home_str.empty() || ws != home_str)) {
            std::string root = (std::filesystem::path(ws) / ".gdocsmith").string();
            mkdir(root.c_str(), 0700);
            chmod(root.c_str(), 0700);

            std::string ignorePath = (std::filesystem::path(root) / ".gitignore").string();
            struct stat ig_st{};
            if (lstat(ignorePath.c_str(), &ig_st) != 0) {
                try {
                    detail::atomic_write(ignorePath, "*\n", 0600);
                } catch (...) {
                    // Ignore EEXIST from concurrent creation
                }
            }
            return root;
        }
    }

    // 3. Per-user private temp directory: <tmp>/gdocsmith-<uid>
    uid_t uid = getuid();
    const char* tmp_env = getenv("TMPDIR");
    std::string tmp_dir = (tmp_env && *tmp_env != '\0') ? tmp_env : "/tmp";
    while (tmp_dir.size() > 1 && tmp_dir.back() == '/') {
        tmp_dir.pop_back();
    }
    std::string root = tmp_dir + "/gdocsmith-" + std::to_string(uid);
    mkdir(root.c_str(), 0700);
    chmod(root.c_str(), 0700);

    struct stat st{};
    if (lstat(root.c_str(), &st) != 0 || !S_ISDIR(st.st_mode) || st.st_uid != uid || (st.st_mode & 0077) != 0) {
        throw std::runtime_error(root + " is not a private directory owned by this user; remove it or set GDOCSMITH_WORK_DIR");
    }
    return root;
}

/// Reads a regular file up to DOCUMENT_FILE_BYTES_MAX without following final symlink
inline FileInputSnapshot input_file_read(const std::string& path) {
    std::filesystem::path p(path);
    if (!p.is_absolute()) {
        throw std::runtime_error("import path must be absolute");
    }

    int fd = open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) {
        throw std::runtime_error("Failed to open file " + path + ": " + strerror(errno));
    }

    struct stat before{};
    if (fstat(fd, &before) != 0) {
        close(fd);
        throw std::runtime_error("Failed to stat " + path + ": " + strerror(errno));
    }

    if (!S_ISREG(before.st_mode) || before.st_size > static_cast<off_t>(DOCUMENT_FILE_BYTES_MAX)) {
        close(fd);
        throw std::runtime_error("import requires a regular file no larger than 16 MiB");
    }

    std::string text;
    text.resize(before.st_size);
    size_t totalRead = 0;
    while (totalRead < static_cast<size_t>(before.st_size)) {
        ssize_t n = read(fd, text.data() + totalRead, before.st_size - totalRead);
        if (n < 0) {
            if (errno == EINTR) continue;
            int err = errno;
            close(fd);
            throw std::runtime_error("Failed to read " + path + ": " + strerror(err));
        }
        if (n == 0) break;
        totalRead += static_cast<size_t>(n);
    }
    text.resize(totalRead);

    struct stat after{};
    if (fstat(fd, &after) != 0) {
        close(fd);
        throw std::runtime_error("Failed to re-stat " + path + ": " + strerror(errno));
    }
    close(fd);

    if (before.st_mtime != after.st_mtime || before.st_size != after.st_size || text.size() > DOCUMENT_FILE_BYTES_MAX) {
        throw std::runtime_error("the import file changed while it was being read");
    }

    char resolved[PATH_MAX];
    char* r = realpath(path.c_str(), resolved);
    std::string real_path = r ? std::string(r) : path;

    return FileInputSnapshot{
        .device = before.st_dev,
        .inode = before.st_ino,
        .hash = detail::sha256_hex(text),
        .path = std::move(real_path),
        .text = std::move(text),
    };
}

/// Checks if a file was replaced, relinked, or edited since input_file_read
inline bool input_file_changed(const FileInputSnapshot& input) {
    struct stat current{};
    if (lstat(input.path.c_str(), &current) != 0) {
        return true;
    }
    if (S_ISLNK(current.st_mode)) {
        return true;
    }
    if (current.st_dev != input.device || current.st_ino != input.inode) {
        return true;
    }
    std::string current_content = detail::read_file_string(input.path);
    return detail::sha256_hex(current_content) != input.hash;
}

/// Replaces a user-chosen file atomically, keeping existing file's mode
inline std::string file_replace(const std::string& path, std::string_view markdown, mode_t defaultMode = 0666) {
    std::filesystem::path p(path);
    if (!p.is_absolute()) {
        throw std::runtime_error("export path must be absolute");
    }

    struct stat st{};
    bool exists = (lstat(path.c_str(), &st) == 0);
    if (exists && !S_ISREG(st.st_mode)) {
        throw std::runtime_error("export path must be a regular file or not exist yet");
    }

    if (p.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(), ec);
    }

    mode_t targetMode = 0666;
    if (exists) {
        targetMode = st.st_mode & 0777;
    } else {
        mode_t curUmask = umask(0);
        umask(curUmask);
        targetMode = defaultMode & ~curUmask;
    }

    detail::atomic_write(path, markdown, targetMode);
    return path;
}

/// Computes the export directory path for a docId and revisionId
inline std::string export_directory(const std::string& root, const std::string& docId, const std::optional<std::string>& revisionId) {
    static const std::regex kValidId("^[A-Za-z0-9_-]+$");
    if (!std::regex_match(docId, kValidId)) {
        throw std::runtime_error("invalid document ID");
    }

    if (!revisionId.has_value() || revisionId->empty()) {
        std::string ts = detail::iso8601_filename_timestamp();
        return (std::filesystem::path(root) / "exports" / docId / ("no-revision." + ts)).string();
    }

    if (!std::regex_match(*revisionId, kValidId)) {
        throw std::runtime_error("the document revision ID contains characters a folder name can't hold");
    }

    return (std::filesystem::path(root) / "exports" / docId / *revisionId).string();
}

/// Writes `exports/<docId>/<revisionId>/{base.md,next.md,comments.yaml,suggestions.yaml}`
inline ExportFolderResult export_folder_write(
    const std::string& root,
    const std::string& docId,
    const std::optional<std::string>& revisionId,
    const std::string& markdown,
    const std::string& commentsYaml = "",
    const std::string& suggestionsYaml = ""
) {
    std::string directory = export_directory(root, docId, revisionId);
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    chmod(directory.c_str(), 0700);

    std::string basePath = (std::filesystem::path(directory) / "base.md").string();
    std::string nextPath = (std::filesystem::path(directory) / "next.md").string();
    std::string exportPath = (std::filesystem::path(directory) / "export.md").string();
    std::string origPath = (std::filesystem::path(directory) / "export.orig.md").string();

    auto currentNext = detail::read_file_string_opt(nextPath);
    if (!currentNext.has_value()) {
        currentNext = detail::read_file_string_opt(exportPath);
    }
    auto previousBase = detail::read_file_string_opt(basePath);
    if (!previousBase.has_value()) {
        previousBase = detail::read_file_string_opt(origPath);
    }

    bool hasLocalEdits = currentNext.has_value() &&
                         *currentNext != markdown &&
                         (!previousBase.has_value() || *currentNext != *previousBase);

    std::vector<std::string> warnings;
    if (hasLocalEdits) {
        warnings.push_back("export.md already had local edits for this revision, so it was kept; delete it and export again for a clean copy.");
    } else {
        detail::atomic_write(nextPath, markdown, 0600);
        detail::atomic_write(exportPath, markdown, 0600);
    }

    // Always update base.md and export.orig.md
    detail::atomic_write(basePath, markdown, 0600);
    detail::atomic_write(origPath, markdown, 0600);

    ExportFolderResult res{
        .exportFilePath = nextPath,
        .baseFilePath = basePath,
        .nextFilePath = nextPath,
        .commentsFilePath = std::nullopt,
        .suggestionsFilePath = std::nullopt,
        .warnings = std::move(warnings),
    };

    if (!commentsYaml.empty()) {
        std::string commentsPath = (std::filesystem::path(directory) / "comments.yaml").string();
        detail::atomic_write(commentsPath, commentsYaml, 0600);
        res.commentsFilePath = commentsPath;
    }

    if (!suggestionsYaml.empty()) {
        std::string suggestionsPath = (std::filesystem::path(directory) / "suggestions.yaml").string();
        detail::atomic_write(suggestionsPath, suggestionsYaml, 0600);
        res.suggestionsFilePath = suggestionsPath;
    }

    return res;
}

} // namespace gdocsmith
