#pragma once

#include "model/errors.hpp"

#include <openssl/evp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gdocsmith::engine {

constexpr size_t DOCUMENT_FILE_BYTES_MAX = 16 * 1024 * 1024; // 16 MiB

struct DeletedTab {
    std::string id;
    std::string title;

    bool operator==(const DeletedTab&) const = default;
};

struct ResultError {
    std::string code;
    std::optional<int> count;
    std::optional<int> line;
    std::optional<std::vector<int>> lines;
    std::string message;
    std::optional<std::string> phase;
    std::optional<std::string> tab;

    bool operator==(const ResultError&) const = default;
};

struct FileWriteResult {
    std::optional<int> commentsLine;
    std::optional<int> suggestionsLine;
    std::optional<std::vector<DeletedTab>> deletedTabs;
    std::optional<std::string> doc;
    std::optional<std::vector<ResultError>> errors;
    std::optional<std::string> exportPath;
    std::string status = "unchanged"; // "applied" | "unchanged" | "refused" | "failed"
    std::string summary;
    std::string verification = "notApplicable"; // "match" | "mismatch" | "inconclusive" | "notApplicable"
    std::vector<std::string> warnings;
    std::string writeState = "none"; // "none" | "complete" | "partial" | "unknown" | "reverted"

    bool operator==(const FileWriteResult&) const = default;
};

struct FileInputSnapshot {
    uint64_t device = 0;
    std::string hash;
    uint64_t inode = 0;
    std::string path;
    std::string text;

    bool operator==(const FileInputSnapshot&) const = default;
};

struct WriteAttempt {
    std::string action;
    std::optional<std::string> createdId;
    std::optional<std::string> doc;
    std::string outcome = "pending"; // "pending" | "confirmed" | "rejected" | "unknown"
    int requestCount = 0;
    std::optional<std::string> revision;

    bool operator==(const WriteAttempt&) const = default;
};

struct ExportFolder {
    std::string exportFilePath;
    std::vector<std::string> warnings;

    bool operator==(const ExportFolder&) const = default;
};

namespace detail {

inline std::mutex g_artifact_mutex;
inline std::optional<std::string> g_workspace_root;

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

inline std::string unique_tmp_suffix() {
    static std::atomic<uint64_t> counter{0};
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    auto c = ++counter;
    auto pid = getpid();
    char buf[64];
    snprintf(buf, sizeof(buf), "%d-%lld-%llu", static_cast<int>(pid), static_cast<long long>(now), static_cast<unsigned long long>(c));
    return std::string(buf);
}

inline std::string get_homedir() {
    const char* h = getenv("HOME");
    if (h && *h) return std::string(h);
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_dir) return std::string(pw->pw_dir);
    return "";
}

inline std::string get_tmpdir() {
    const char* t = getenv("TMPDIR");
    if (t && *t) {
        std::string s(t);
        while (s.size() > 1 && s.back() == '/') s.pop_back();
        return s;
    }
    return "/tmp";
}

inline std::string temporary_root() {
    uid_t uid = getuid();
    std::string username = std::to_string(uid);
    struct passwd* pw = getpwuid(uid);
    if (pw && pw->pw_name) {
        username = pw->pw_name;
    }
    std::string root = get_tmpdir() + "/gdocsmith-" + username;
    mkdir(root.c_str(), 0700);

    struct stat st{};
    if (lstat(root.c_str(), &st) != 0 || !S_ISDIR(st.st_mode) || st.st_uid != uid || (st.st_mode & 0077) != 0) {
        throw std::runtime_error(root + " is not a private directory owned by this user; remove it or set GDOCSMITH_WORK_DIR");
    }
    return root;
}

inline void atomic_write(const std::string& path, std::string_view value, mode_t mode = 0600) {
    std::string temporary = path + "." + unique_tmp_suffix() + ".tmp";
    int fd = open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, mode);
    if (fd < 0) {
        throw std::runtime_error("Failed to open temporary file " + temporary + ": " + strerror(errno));
    }
    size_t total = 0;
    while (total < value.size()) {
        ssize_t n = write(fd, value.data() + total, value.size() - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            int err = errno;
            close(fd);
            unlink(temporary.c_str());
            throw std::runtime_error("Failed to write to " + temporary + ": " + strerror(err));
        }
        total += static_cast<size_t>(n);
    }
    fsync(fd);
    close(fd);
    if (rename(temporary.c_str(), path.c_str()) != 0) {
        int err = errno;
        unlink(temporary.c_str());
        throw std::runtime_error("Failed to rename " + temporary + " to " + path + ": " + strerror(err));
    }
}

inline std::optional<std::string> file_text_read(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return std::nullopt;
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

inline std::string timestamp_folder_suffix() {
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&tt, &tm);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H-%M-%SZ", &tm);
    return std::string(buf);
}

inline mode_t current_umask() {
    mode_t mask = umask(0);
    umask(mask);
    return mask;
}

} // namespace detail

inline void artifactWorkspaceSet(const std::optional<std::string>& root) {
    std::lock_guard<std::mutex> lock(detail::g_artifact_mutex);
    if (root.has_value() && !root->empty()) {
        std::error_code ec;
        auto canonical = std::filesystem::weakly_canonical(*root, ec);
        detail::g_workspace_root = ec ? *root : canonical.string();
    } else {
        detail::g_workspace_root = std::nullopt;
    }
}

inline std::string artifactRoot() {
    std::lock_guard<std::mutex> lock(detail::g_artifact_mutex);
    const char* env = getenv("GDOCSMITH_WORK_DIR");
    if (env && *env) {
        std::error_code ec;
        auto canonical = std::filesystem::weakly_canonical(env, ec);
        return ec ? std::string(env) : canonical.string();
    }

    if (!detail::g_workspace_root.has_value() || detail::g_workspace_root->empty()) {
        return detail::temporary_root();
    }

    std::filesystem::path ws(*detail::g_workspace_root);
    std::error_code ec;
    std::filesystem::path normWs = std::filesystem::weakly_canonical(ws, ec);
    std::string wsStr = ec ? ws.string() : normWs.string();

    std::string home = detail::get_homedir();
    if (!home.empty()) {
        std::error_code homeEc;
        auto normHome = std::filesystem::weakly_canonical(home, homeEc);
        if (!homeEc && wsStr == normHome.string()) {
            return detail::temporary_root();
        }
    }

    if (wsStr == "/" || ws.root_path() == ws) {
        return detail::temporary_root();
    }

    std::filesystem::path rootPath = ws / ".gdocsmith";
    std::filesystem::create_directories(rootPath, ec);
    chmod(rootPath.c_str(), 0700);

    std::filesystem::path ignorePath = rootPath / ".gitignore";
    int fd = open(ignorePath.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (fd >= 0) {
        const char content[] = "*\n";
        (void)write(fd, content, sizeof(content) - 1);
        close(fd);
    }

    return rootPath.string();
}

inline FileInputSnapshot inputFileRead(const std::string& path) {
    std::filesystem::path p(path);
    if (!p.is_absolute()) {
        throw model::CoreError(std::string(model::error_codes::UNSUPPORTED_SYNTAX), "import path must be absolute");
    }

    int fd = open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) {
        throw model::CoreError(std::string(model::error_codes::UNSUPPORTED_SYNTAX), "cannot open import file: " + path);
    }

    struct stat before{};
    if (fstat(fd, &before) != 0 || !S_ISREG(before.st_mode) || before.st_size > static_cast<off_t>(DOCUMENT_FILE_BYTES_MAX)) {
        close(fd);
        throw model::CoreError(std::string(model::error_codes::UNSUPPORTED_SYNTAX), "import requires a regular file no larger than 16 MiB");
    }

    std::string bytes(static_cast<size_t>(before.st_size), '\0');
    size_t total = 0;
    while (total < bytes.size()) {
        ssize_t n = read(fd, bytes.data() + total, bytes.size() - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            close(fd);
            throw model::CoreError(std::string(model::error_codes::UNSUPPORTED_SYNTAX), "failed reading import file");
        }
        if (n == 0) break;
        total += static_cast<size_t>(n);
    }

    struct stat after{};
    if (fstat(fd, &after) != 0 || before.st_mtime != after.st_mtime || before.st_size != after.st_size || bytes.size() > DOCUMENT_FILE_BYTES_MAX) {
        close(fd);
        throw model::CoreError(std::string(model::error_codes::STALE_BASE), "the import file changed while it was being read");
    }
    close(fd);

    char realPathBuf[PATH_MAX]{};
    std::string canonicalPath = path;
    if (realpath(path.c_str(), realPathBuf) != nullptr) {
        canonicalPath = realPathBuf;
    }

    return FileInputSnapshot{
        .device = static_cast<uint64_t>(before.st_dev),
        .hash = detail::sha256_hex(bytes),
        .inode = static_cast<uint64_t>(before.st_ino),
        .path = canonicalPath,
        .text = std::move(bytes),
    };
}

inline ExportFolder exportFolderWrite(
    const std::string& root,
    const std::string& docId,
    const std::optional<std::string>& revisionId,
    const std::string& markdown
) {
    static const std::regex idRegex("^[A-Za-z0-9_-]+$");
    if (!std::regex_match(docId, idRegex)) {
        throw model::CoreError(std::string(model::error_codes::UNSUPPORTED_SYNTAX), "invalid document ID");
    }

    std::filesystem::path dir = std::filesystem::path(root) / "exports" / docId;
    if (revisionId.has_value()) {
        if (!std::regex_match(*revisionId, idRegex)) {
            throw model::CoreError(std::string(model::error_codes::INTERNAL), "the document revision ID contains characters a folder name can't hold");
        }
        dir /= *revisionId;
    } else {
        dir /= "no-revision." + detail::timestamp_folder_suffix();
    }

    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    chmod(dir.c_str(), 0700);

    std::filesystem::path exportFilePath = dir / "export.md";
    std::filesystem::path origPath = dir / "export.orig.md";

    auto body = detail::file_text_read(exportFilePath.string());
    auto previous = detail::file_text_read(origPath.string());

    bool edited = body.has_value() && *body != markdown && (!previous.has_value() || *body != *previous);
    std::vector<std::string> warnings;
    if (edited) {
        warnings.push_back("export.md already had local edits for this revision, so it was kept; delete it and export again for a clean copy.");
    } else {
        detail::atomic_write(exportFilePath.string(), markdown);
    }
    detail::atomic_write(origPath.string(), markdown);

    return ExportFolder{
        .exportFilePath = exportFilePath.string(),
        .warnings = std::move(warnings),
    };
}

inline std::string fileReplace(const std::string& path, const std::string& markdown) {
    std::filesystem::path p(path);
    if (!p.is_absolute()) {
        throw model::CoreError(std::string(model::error_codes::UNSUPPORTED_SYNTAX), "export path must be absolute");
    }

    struct stat st{};
    bool exists = (lstat(path.c_str(), &st) == 0);
    if (exists && !S_ISREG(st.st_mode)) {
        throw model::CoreError(std::string(model::error_codes::UNSUPPORTED_SYNTAX), "export path must be a regular file or not exist yet");
    }

    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);

    mode_t mode = exists ? (st.st_mode & 0777) : (0666 & ~detail::current_umask());
    detail::atomic_write(path, markdown, mode);
    return path;
}

inline bool inputFileChanged(const FileInputSnapshot& input) {
    struct stat current{};
    if (lstat(input.path.c_str(), &current) != 0) {
        return true;
    }
    if (S_ISLNK(current.st_mode) ||
        static_cast<uint64_t>(current.st_dev) != input.device ||
        static_cast<uint64_t>(current.st_ino) != input.inode) {
        return true;
    }
    auto content = detail::file_text_read(input.path);
    if (!content.has_value()) return true;
    return detail::sha256_hex(*content) != input.hash;
}

class WriteTracker {
public:
    std::map<std::string, std::string> revisions;
    std::map<std::string, slimr::Json> documents;
    std::vector<WriteAttempt> writes;

    [[nodiscard]] std::string writeState(bool complete) const {
        for (const auto& w : writes) {
            if (w.outcome == "unknown" || w.outcome == "pending") return "unknown";
        }
        bool hasConfirmed = false;
        for (const auto& w : writes) {
            if (w.outcome == "confirmed") {
                hasConfirmed = true;
                break;
            }
        }
        if (!hasConfirmed) return "none";
        return complete ? "complete" : "partial";
    }
};

} // namespace gdocsmith::engine
