#pragma once

#include "curl.hpp"
#include "json.hpp"

#include <poll.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

extern char** environ;

namespace gdocsmith {

class CoreError : public std::runtime_error {
public:
    std::string code;
    CoreError(std::string_view c, std::string_view msg)
        : std::runtime_error(std::string(msg)), code(c) {}
};

struct HttpCall {
    std::string method = "GET";
    std::string url;
    std::map<std::string, std::string> headers;
    std::string body;
    long timeoutMs = 30000;
};

struct HttpCallResponse {
    long status = 200;
    std::map<std::string, std::string> headers;
    std::string body;
};

using HttpTransport = std::function<HttpCallResponse(const HttpCall&)>;
using GwsRunner = std::function<std::string(const std::vector<std::string>& args)>;

inline HttpCallResponse default_curl_transport(const HttpCall& req) {
    slimr::HttpRequest r;
    r.method = req.method;
    r.url = req.url;
    r.headers = req.headers;
    r.body = req.body;
    r.timeoutMs = req.timeoutMs;
    auto resp = slimr::request(r);
    return HttpCallResponse{
        .status = resp.status,
        .headers = std::move(resp.headers),
        .body = std::move(resp.body),
    };
}

namespace detail {

struct UniqueFd {
    int fd = -1;
    UniqueFd() = default;
    explicit UniqueFd(int f) noexcept : fd(f) {}
    ~UniqueFd() { reset(); }
    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;
    UniqueFd(UniqueFd&& other) noexcept : fd(std::exchange(other.fd, -1)) {}
    UniqueFd& operator=(UniqueFd&& other) noexcept {
        if (this != &other) {
            reset();
            fd = std::exchange(other.fd, -1);
        }
        return *this;
    }
    void reset(int next = -1) noexcept {
        if (fd >= 0) {
            close(fd);
        }
        fd = next;
    }
    int get() const noexcept { return fd; }
    explicit operator bool() const noexcept { return fd >= 0; }
};

struct SpawnActionsGuard {
    posix_spawn_file_actions_t actions{};
    bool active = false;
    SpawnActionsGuard() {
        if (posix_spawn_file_actions_init(&actions) != 0) {
            throw std::runtime_error("posix_spawn_file_actions_init failed");
        }
        active = true;
    }
    ~SpawnActionsGuard() {
        if (active) {
            posix_spawn_file_actions_destroy(&actions);
        }
    }
    SpawnActionsGuard(const SpawnActionsGuard&) = delete;
    SpawnActionsGuard& operator=(const SpawnActionsGuard&) = delete;
};

inline void write_uint16_le(std::string& out, uint16_t val) {
    out.push_back(static_cast<char>(val & 0xff));
    out.push_back(static_cast<char>((val >> 8) & 0xff));
}

inline void write_uint32_le(std::string& out, uint32_t val) {
    out.push_back(static_cast<char>(val & 0xff));
    out.push_back(static_cast<char>((val >> 8) & 0xff));
    out.push_back(static_cast<char>((val >> 16) & 0xff));
    out.push_back(static_cast<char>((val >> 24) & 0xff));
}

} // namespace detail

inline std::string exec_gws(const std::vector<std::string>& args) {
    const char* bin_env = getenv("GWS_BIN");
    std::string bin = (bin_env && *bin_env) ? bin_env : "gws";

    int stdout_raw[2]{-1, -1};
    int stderr_raw[2]{-1, -1};
    if (pipe(stdout_raw) != 0) {
        throw std::runtime_error("pipe failed: " + std::string(strerror(errno)));
    }
    detail::UniqueFd stdout_read(stdout_raw[0]);
    detail::UniqueFd stdout_write(stdout_raw[1]);

    if (pipe(stderr_raw) != 0) {
        throw std::runtime_error("pipe failed: " + std::string(strerror(errno)));
    }
    detail::UniqueFd stderr_read(stderr_raw[0]);
    detail::UniqueFd stderr_write(stderr_raw[1]);

    detail::SpawnActionsGuard actions;
    posix_spawn_file_actions_addclose(&actions.actions, stdout_read.get());
    posix_spawn_file_actions_adddup2(&actions.actions, stdout_write.get(), STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions.actions, stdout_write.get());

    posix_spawn_file_actions_addclose(&actions.actions, stderr_read.get());
    posix_spawn_file_actions_adddup2(&actions.actions, stderr_write.get(), STDERR_FILENO);
    posix_spawn_file_actions_addclose(&actions.actions, stderr_write.get());

    std::vector<char*> argv_ptrs;
    argv_ptrs.reserve(args.size() + 2);
    argv_ptrs.push_back(const_cast<char*>(bin.c_str()));
    for (const auto& a : args) {
        argv_ptrs.push_back(const_cast<char*>(a.c_str()));
    }
    argv_ptrs.push_back(nullptr);

    pid_t pid = -1;
    int spawn_ret = posix_spawnp(&pid, bin.c_str(), &actions.actions, nullptr, argv_ptrs.data(), environ);

    stdout_write.reset();
    stderr_write.reset();

    if (spawn_ret != 0) {
        std::string hint = (spawn_ret == ENOENT) ? (" (is " + bin + " installed and on PATH? set GWS_BIN to override)") : "";
        throw std::runtime_error("Could not run " + bin + ": " + strerror(spawn_ret) + hint);
    }

    std::string stdout_str;
    std::string stderr_str;
    struct pollfd fds[2]{};
    fds[0].fd = stdout_read.get();
    fds[0].events = POLLIN;
    fds[1].fd = stderr_read.get();
    fds[1].events = POLLIN;

    char buf[16384];
    int open_fds = 2;
    while (open_fds > 0) {
        int poll_res = poll(fds, 2, -1);
        if (poll_res < 0) {
            if (errno == EINTR) continue;
            break;
        }
        for (int i = 0; i < 2; ++i) {
            if (fds[i].fd >= 0 && (fds[i].revents & (POLLIN | POLLHUP | POLLERR))) {
                ssize_t n = read(fds[i].fd, buf, sizeof(buf));
                if (n > 0) {
                    if (i == 0) stdout_str.append(buf, static_cast<size_t>(n));
                    else stderr_str.append(buf, static_cast<size_t>(n));
                } else if (n < 0 && errno == EINTR) {
                    continue;
                } else {
                    if (i == 0) stdout_read.reset();
                    else stderr_read.reset();
                    fds[i].fd = -1;
                    open_fds--;
                }
            }
        }
    }

    int status = 0;
    pid_t waited = -1;
    do {
        waited = waitpid(pid, &status, 0);
    } while (waited < 0 && errno == EINTR);
    if (waited < 0) {
        throw std::runtime_error("waitpid failed: " + std::string(strerror(errno)));
    }

    if (WIFEXITED(status)) {
        int exitCode = WEXITSTATUS(status);
        if (exitCode != 0) {
            throw std::runtime_error("gws failed (exit " + std::to_string(exitCode) + "): " + stderr_str);
        }
    } else if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        throw std::runtime_error("gws terminated by signal " + std::to_string(sig));
    }

    return stdout_str;
}

// OAuth Credentials & Tokens
struct GwsCredentials {
    std::string clientId;
    std::string clientSecret;
    std::string refreshToken;
    std::string type;
};

struct CachedToken {
    std::string accessToken;
    int64_t expiresAtMs = 0;
    std::string tokenType;
};

namespace detail {

inline std::mutex g_auth_mutex;
inline std::optional<GwsCredentials> g_cached_credentials;
inline std::optional<CachedToken> g_cached_token;

inline std::string url_encode(std::string_view sv) {
    return slimr::url_encode(sv);
}

} // namespace detail

inline void token_cache_reset() {
    std::lock_guard<std::mutex> lock(detail::g_auth_mutex);
    detail::g_cached_credentials = std::nullopt;
    detail::g_cached_token = std::nullopt;
}

inline GwsCredentials gws_credentials_get(GwsRunner runner = nullptr) {
    std::lock_guard<std::mutex> lock(detail::g_auth_mutex);
    if (detail::g_cached_credentials.has_value()) {
        return *detail::g_cached_credentials;
    }

    std::string stdout_str;
    try {
        if (runner) {
            stdout_str = runner({"auth", "export", "--unmasked"});
        } else {
            stdout_str = exec_gws({"auth", "export", "--unmasked"});
        }
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to export credentials from gws: " + std::string(e.what()));
    }

    auto parsedOpt = slimr::parse_json<slimr::Json>(stdout_str);
    if (!parsedOpt) {
        throw std::runtime_error("Invalid gws auth export output: invalid JSON");
    }
    const auto& j = *parsedOpt;
    if (!j.contains("client_id") || !j["client_id"].is_string() ||
        !j.contains("client_secret") || !j["client_secret"].is_string() ||
        !j.contains("refresh_token") || !j["refresh_token"].is_string()) {
        throw std::runtime_error("Missing required OAuth fields in gws credentials");
    }

    GwsCredentials creds{
        .clientId = j["client_id"].get<std::string>(),
        .clientSecret = j["client_secret"].get<std::string>(),
        .refreshToken = j["refresh_token"].get<std::string>(),
        .type = j.contains("type") && j["type"].is_string() ? j["type"].get<std::string>() : "",
    };

    detail::g_cached_credentials = creds;
    return creds;
}

inline CachedToken access_token_refresh(
    std::optional<GwsCredentials> credsOpt = std::nullopt,
    HttpTransport transport = nullptr,
    GwsRunner runner = nullptr
) {
    GwsCredentials creds = credsOpt.has_value() ? *credsOpt : gws_credentials_get(runner);

    std::string post_data = "client_id=" + detail::url_encode(creds.clientId) +
                            "&client_secret=" + detail::url_encode(creds.clientSecret) +
                            "&grant_type=refresh_token" +
                            "&refresh_token=" + detail::url_encode(creds.refreshToken);

    HttpCall req{
        .method = "POST",
        .url = "https://oauth2.googleapis.com/token",
        .headers = {{"Content-Type", "application/x-www-form-urlencoded"}},
        .body = std::move(post_data),
        .timeoutMs = 15000,
    };

    HttpCallResponse res;
    if (transport) {
        res = transport(req);
    } else {
        res = default_curl_transport(req);
    }

    if (res.status != 200) {
        throw std::runtime_error("Google OAuth token refresh failed (" + std::to_string(res.status) + "): " + res.body);
    }

    auto parsedOpt = slimr::parse_json<slimr::Json>(res.body);
    if (!parsedOpt) {
        throw std::runtime_error("No access_token returned by Google OAuth endpoint: invalid JSON response");
    }
    const auto& j = *parsedOpt;
    if (!j.contains("access_token") || !j["access_token"].is_string()) {
        throw std::runtime_error("No access_token returned by Google OAuth endpoint");
    }

    int64_t expiresInSec = 3600;
    if (j.contains("expires_in") && j["expires_in"].is_number()) {
        expiresInSec = j["expires_in"].get<int64_t>();
    }

    auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    // 60-second safety margin
    int64_t expiresInMs = std::max<int64_t>(0, (expiresInSec - 60) * 1000);

    CachedToken tok{
        .accessToken = j["access_token"].get<std::string>(),
        .expiresAtMs = nowMs + expiresInMs,
        .tokenType = j.contains("token_type") && j["token_type"].is_string() ? j["token_type"].get<std::string>() : "Bearer",
    };

    std::lock_guard<std::mutex> lock(detail::g_auth_mutex);
    detail::g_cached_token = tok;
    return tok;
}

inline std::string valid_access_token_get(
    bool forceRefresh = false,
    HttpTransport transport = nullptr,
    GwsRunner runner = nullptr
) {
    auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    {
        std::lock_guard<std::mutex> lock(detail::g_auth_mutex);
        if (!forceRefresh && detail::g_cached_token.has_value() && detail::g_cached_token->expiresAtMs > nowMs) {
            return detail::g_cached_token->accessToken;
        }
    }

    return access_token_refresh(std::nullopt, transport, runner).accessToken;
}

/// Parses and formats Google Workspace / HTTP errors into actionable messages
inline std::string gws_error_format(std::string_view raw, std::string_view targetId = "") {
    size_t start = raw.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) {
        return !targetId.empty() ? ("GWS operation failed for target " + std::string(targetId)) : "GWS operation failed";
    }
    size_t end = raw.find_last_not_of(" \t\r\n");
    std::string trimmed(raw.substr(start, end - start + 1));

    std::string apiMessage;
    std::string statusStr;
    int64_t statusNum = 0;

    auto parsedOpt = slimr::parse_json<slimr::Json>(trimmed);
    if (!parsedOpt) {
        // Try regex snippet for nested error json
        static const std::regex errRegex(R"(\{[^{}]*"error"[^{}]*\})");
        std::smatch m;
        if (std::regex_search(trimmed, m, errRegex)) {
            parsedOpt = slimr::parse_json<slimr::Json>(m[0].str());
        }
    }

    if (parsedOpt && parsedOpt->contains("error") && (*parsedOpt)["error"].is_object()) {
        const auto& errObj = (*parsedOpt)["error"];
        if (errObj.contains("code") && errObj["code"].is_number()) {
            statusNum = errObj["code"].get<int64_t>();
            statusStr = std::to_string(statusNum);
        } else if (errObj.contains("status") && errObj["status"].is_string()) {
            statusStr = errObj["status"].get<std::string>();
        }
        if (errObj.contains("message") && errObj["message"].is_string()) {
            apiMessage = errObj["message"].get<std::string>();
        }
    }

    std::string combined = trimmed + " " + apiMessage;

    // 403 Forbidden / Permission denied
    static const std::regex permRegex(R"(insufficient(File)?Permissions|does not have permission|not have write access|Access denied)", std::regex_constants::icase);
    if (statusNum == 403 || std::regex_search(combined, permRegex)) {
        std::string idHint = !targetId.empty() ? (" on \"" + std::string(targetId) + "\"") : "";
        std::string detail = !apiMessage.empty() ? (": " + apiMessage) : "";
        return "Permission denied" + idHint + ". You do not have sufficient permissions (e.g. view-only access, or restricted shared drive)" + detail + ".";
    }

    // 404 Not Found
    static const std::regex notFoundRegex(R"(notFound|File not found|Requested entity was not found)", std::regex_constants::icase);
    if (statusNum == 404 || std::regex_search(combined, notFoundRegex)) {
        std::string idHint = !targetId.empty() ? (" \"" + std::string(targetId) + "\"") : "";
        return "Document or file not found" + idHint + ". Check that the ID or URL is correct and shared with your account.";
    }

    // 401 Unauthorized
    static const std::regex authRegex(R"(invalid_grant|invalid authentication credentials|Unauthorized|Login Required)", std::regex_constants::icase);
    if (statusNum == 401 || std::regex_search(combined, authRegex)) {
        return "Google Workspace authentication expired or invalid. Run 'gws auth login' to re-authenticate.";
    }

    if (!apiMessage.empty()) {
        std::string codeStr = !statusStr.empty() ? statusStr : "error";
        return "Google API error (" + codeStr + "): " + apiMessage;
    }

    return trimmed;
}

/// Decodes HTML entities in quoted text
inline std::string entities_decode(std::string_view text) {
    std::string res;
    res.reserve(text.size());
    for (size_t i = 0; i < text.size();) {
        if (text[i] == '&') {
            size_t semi = text.find(';', i + 1);
            if (semi != std::string_view::npos && semi - i <= 10) {
                std::string_view entity = text.substr(i + 1, semi - i - 1);
                if (entity == "quot") { res += '"'; i = semi + 1; continue; }
                if (entity == "apos" || entity == "#39") { res += '\''; i = semi + 1; continue; }
                if (entity == "lt") { res += '<'; i = semi + 1; continue; }
                if (entity == "gt") { res += '>'; i = semi + 1; continue; }
                if (entity == "amp") { res += '&'; i = semi + 1; continue; }
                if (entity.starts_with("#x") || entity.starts_with("#X")) {
                    char* endptr = nullptr;
                    std::string hexStr(entity.substr(2));
                    unsigned long cp = std::strtoul(hexStr.c_str(), &endptr, 16);
                    if (endptr && *endptr == '\0' && cp <= 0x10FFFF) {
                        if (cp <= 0x7F) {
                            res += static_cast<char>(cp);
                        } else if (cp <= 0x7FF) {
                            res += static_cast<char>(0xC0 | ((cp >> 6) & 0x1F));
                            res += static_cast<char>(0x80 | (cp & 0x3F));
                        } else if (cp <= 0xFFFF) {
                            res += static_cast<char>(0xE0 | ((cp >> 12) & 0x0F));
                            res += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                            res += static_cast<char>(0x80 | (cp & 0x3F));
                        } else {
                            res += static_cast<char>(0xF0 | ((cp >> 18) & 0x07));
                            res += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                            res += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                            res += static_cast<char>(0x80 | (cp & 0x3F));
                        }
                        i = semi + 1;
                        continue;
                    }
                } else if (entity.starts_with("#")) {
                    char* endptr = nullptr;
                    std::string decStr(entity.substr(1));
                    unsigned long cp = std::strtoul(decStr.c_str(), &endptr, 10);
                    if (endptr && *endptr == '\0' && cp <= 0x10FFFF) {
                        if (cp <= 0x7F) {
                            res += static_cast<char>(cp);
                        } else if (cp <= 0x7FF) {
                            res += static_cast<char>(0xC0 | ((cp >> 6) & 0x1F));
                            res += static_cast<char>(0x80 | (cp & 0x3F));
                        } else if (cp <= 0xFFFF) {
                            res += static_cast<char>(0xE0 | ((cp >> 12) & 0x0F));
                            res += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                            res += static_cast<char>(0x80 | (cp & 0x3F));
                        } else {
                            res += static_cast<char>(0xF0 | ((cp >> 18) & 0x07));
                            res += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                            res += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                            res += static_cast<char>(0x80 | (cp & 0x3F));
                        }
                        i = semi + 1;
                        continue;
                    }
                }
            }
        }
        res += text[i];
        i++;
    }
    return res;
}

/// Normalizes URL or doc ID string to clean document ID
inline std::string document_id(std::string_view reference) {
    static const std::regex urlRegex(R"(https?://docs\.google\.com/document/d/([A-Za-z0-9_-]+))");
    std::string s(reference);
    std::smatch m;
    if (std::regex_search(s, m, urlRegex)) {
        return m[1].str();
    }
    static const std::regex idRegex("^[A-Za-z0-9_-]+$");
    if (std::regex_match(s, idRegex)) {
        return s;
    }
    throw CoreError("unsupportedSyntax", "invalid document ID or URL: " + s);
}

// Drive Permission Types
struct DrivePermission {
    std::string id;
    std::string role; // "commenter", "fileOrganizer", "organizer", "owner", "reader", "writer"
    std::string type; // "anyone", "domain", "group", "internal", "user"
    std::optional<std::string> displayName;
    std::optional<std::string> domain;
    std::optional<std::string> emailAddress;
};

struct DrivePermissionCreateOptions {
    std::optional<std::string> emailMessage;
    std::optional<bool> moveToNewOwnersRoot;
    std::optional<bool> sendNotificationEmail;
    std::optional<bool> transferOwnership;
};

// Drive Comment Types
struct DriveCommentAuthor {
    std::optional<std::string> displayName;
    std::optional<std::string> emailAddress;
    std::optional<bool> me;
};

struct DriveCommentReply {
    std::string id;
    std::optional<std::string> content;
    std::optional<bool> deleted;
    std::optional<std::string> action;
    std::optional<DriveCommentAuthor> author;
    std::optional<std::string> createdTime;
    std::optional<std::string> modifiedTime;
};

struct DriveCommentQuotedContent {
    std::optional<std::string> value;
};

struct DriveComment {
    std::string id;
    std::optional<std::string> content;
    std::optional<DriveCommentAuthor> author;
    std::optional<std::string> createdTime;
    std::optional<std::string> modifiedTime;
    std::optional<bool> deleted;
    std::optional<bool> resolved;
    std::optional<std::string> quotedText;
    std::optional<DriveCommentQuotedContent> quotedFileContent;
    std::vector<DriveCommentReply> replies;
};

// Drive File Metadata
struct DriveFile {
    std::string id;
    std::string name;
    std::string mimeType;
    std::optional<bool> trashed;
    std::vector<std::string> parents;
    std::optional<std::string> webViewLink;
    std::optional<std::string> createdTime;
    std::optional<std::string> modifiedTime;
    std::optional<std::string> version;
    std::optional<int64_t> size;
};

// Docx and Image Types
struct DocxImage {
    std::string bytes;
    int widthPx = 0;
    int heightPx = 0;
    std::string type; // "gif", "jpeg", "png"
};

struct DocxImageInfo {
    int widthPx = 0;
    int heightPx = 0;
    std::string type; // "gif", "jpeg", "png"
};

struct ImagesUploadResult {
    std::vector<std::string> contentUris;
    std::string documentId;
};

/// An image's type and pixel dimensions from its header bytes; returns nullopt when not PNG, GIF, or JPEG.
inline std::optional<DocxImageInfo> image_info(std::string_view bytes) {
    auto u8 = [](char c) -> uint8_t { return static_cast<uint8_t>(c); };
    if (bytes.size() > 24 &&
        u8(bytes[0]) == 0x89 && u8(bytes[1]) == 0x50 && u8(bytes[2]) == 0x4e && u8(bytes[3]) == 0x47) {
        uint32_t width = (static_cast<uint32_t>(u8(bytes[16])) << 24) |
                         (static_cast<uint32_t>(u8(bytes[17])) << 16) |
                         (static_cast<uint32_t>(u8(bytes[18])) << 8) |
                         static_cast<uint32_t>(u8(bytes[19]));
        uint32_t height = (static_cast<uint32_t>(u8(bytes[20])) << 24) |
                          (static_cast<uint32_t>(u8(bytes[21])) << 16) |
                          (static_cast<uint32_t>(u8(bytes[22])) << 8) |
                          static_cast<uint32_t>(u8(bytes[23]));
        return DocxImageInfo{
            .widthPx = static_cast<int>(width),
            .heightPx = static_cast<int>(height),
            .type = "png",
        };
    }
    if (bytes.size() > 10 && bytes.starts_with("GIF8")) {
        uint16_t width = static_cast<uint16_t>(u8(bytes[6])) | (static_cast<uint16_t>(u8(bytes[7])) << 8);
        uint16_t height = static_cast<uint16_t>(u8(bytes[8])) | (static_cast<uint16_t>(u8(bytes[9])) << 8);
        return DocxImageInfo{
            .widthPx = static_cast<int>(width),
            .heightPx = static_cast<int>(height),
            .type = "gif",
        };
    }
    if (bytes.size() > 4 && u8(bytes[0]) == 0xff && u8(bytes[1]) == 0xd8) {
        size_t at = 2;
        while (at + 9 < bytes.size() && u8(bytes[at]) == 0xff) {
            uint8_t marker = u8(bytes[at + 1]);
            uint16_t length = (static_cast<uint16_t>(u8(bytes[at + 2])) << 8) | static_cast<uint16_t>(u8(bytes[at + 3]));
            if (marker >= 0xc0 && marker <= 0xcf && marker != 0xc4 && marker != 0xc8 && marker != 0xcc) {
                uint16_t height = (static_cast<uint16_t>(u8(bytes[at + 5])) << 8) | static_cast<uint16_t>(u8(bytes[at + 6]));
                uint16_t width = (static_cast<uint16_t>(u8(bytes[at + 7])) << 8) | static_cast<uint16_t>(u8(bytes[at + 8]));
                return DocxImageInfo{
                    .widthPx = static_cast<int>(width),
                    .heightPx = static_cast<int>(height),
                    .type = "jpeg",
                };
            }
            at += 2 + length;
        }
    }
    return std::nullopt;
}

/// The ZIP CRC-32 of data
inline uint32_t zip_crc32(std::string_view data) {
    uint32_t crc = 0xffffffff;
    for (unsigned char byte : data) {
        crc ^= byte;
        for (int k = 0; k < 8; ++k) {
            crc = (crc & 1) ? ((crc >> 1) ^ 0xedb88320U) : (crc >> 1);
        }
    }
    return crc ^ 0xffffffff;
}

/// Builds an uncompressed (stored) ZIP archive matching Node.js zipStore exactly
inline std::string zip_store(const std::vector<std::pair<std::string, std::string>>& files) {
    std::string locals;
    std::string centrals;
    uint32_t offset = 0;

    for (const auto& [name, data] : files) {
        uint32_t crc = zip_crc32(data);
        uint32_t dataLen = static_cast<uint32_t>(data.size());
        uint16_t nameLen = static_cast<uint16_t>(name.size());

        // Local header (30 bytes)
        std::string local;
        local.reserve(30);
        detail::write_uint32_le(local, 0x04034b50); // signature
        detail::write_uint16_le(local, 20);         // version needed: 20
        detail::write_uint16_le(local, 0);          // flags: 0
        detail::write_uint16_le(local, 0);          // compression: 0 (store)
        detail::write_uint16_le(local, 0);          // time: 0
        detail::write_uint16_le(local, 0);          // date: 0
        detail::write_uint32_le(local, crc);        // crc-32
        detail::write_uint32_le(local, dataLen);    // compressed size
        detail::write_uint32_le(local, dataLen);    // uncompressed size
        detail::write_uint16_le(local, nameLen);    // file name length
        detail::write_uint16_le(local, 0);          // extra field length

        // Central directory entry (46 bytes)
        std::string central;
        central.reserve(46);
        detail::write_uint32_le(central, 0x02014b50); // signature
        detail::write_uint16_le(central, 20);         // version made by: 20
        detail::write_uint16_le(central, 20);         // version needed: 20
        detail::write_uint16_le(central, 0);          // flags: 0
        detail::write_uint16_le(central, 0);          // compression: 0
        detail::write_uint16_le(central, 0);          // time: 0
        detail::write_uint16_le(central, 0);          // date: 0
        detail::write_uint32_le(central, crc);        // crc-32
        detail::write_uint32_le(central, dataLen);    // compressed size
        detail::write_uint32_le(central, dataLen);    // uncompressed size
        detail::write_uint16_le(central, nameLen);    // file name length
        detail::write_uint16_le(central, 0);          // extra field length
        detail::write_uint16_le(central, 0);          // comment length
        detail::write_uint16_le(central, 0);          // disk number start
        detail::write_uint16_le(central, 0);          // internal attrs
        detail::write_uint32_le(central, 0);          // external attrs
        detail::write_uint32_le(central, offset);     // relative offset of local header

        locals.append(local);
        locals.append(name);
        locals.append(data);

        centrals.append(central);
        centrals.append(name);

        offset += static_cast<uint32_t>(local.size() + name.size() + data.size());
    }

    uint32_t centralDirSize = static_cast<uint32_t>(centrals.size());
    uint32_t centralDirOffset = offset;
    uint16_t fileCount = static_cast<uint16_t>(files.size());

    // End of central directory record (22 bytes)
    std::string end;
    end.reserve(22);
    detail::write_uint32_le(end, 0x06054b50);       // signature
    detail::write_uint16_le(end, 0);                // disk number
    detail::write_uint16_le(end, 0);                // disk with central dir
    detail::write_uint16_le(end, fileCount);        // entries on disk
    detail::write_uint16_le(end, fileCount);        // total entries
    detail::write_uint32_le(end, centralDirSize);   // size of central dir
    detail::write_uint32_le(end, centralDirOffset); // offset of central dir
    detail::write_uint16_le(end, 0);                // comment length

    std::string result;
    result.reserve(locals.size() + centrals.size() + end.size());
    result.append(locals);
    result.append(centrals);
    result.append(end);
    return result;
}

/// A .docx package around a document body with images appended as one inline picture per paragraph
inline std::string docx_build(const std::string& body, const std::vector<DocxImage>& images = {}) {
    std::string ns = "xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\" "
                     "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" "
                     "xmlns:wp=\"http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing\" "
                     "xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
                     "xmlns:pic=\"http://schemas.openxmlformats.org/drawingml/2006/picture\"";
    std::string pictures;
    for (size_t i = 0; i < images.size(); ++i) {
        const auto& image = images[i];
        int64_t cx = static_cast<int64_t>(image.widthPx) * 9525;
        int64_t cy = static_cast<int64_t>(image.heightPx) * 9525;
        std::string name = "image" + std::to_string(i + 1);
        std::string idStr = std::to_string(i + 1);
        std::string embedId = "img" + std::to_string(i);
        pictures += "<w:p><w:r><w:drawing><wp:inline><wp:extent cx=\"" + std::to_string(cx) +
                    "\" cy=\"" + std::to_string(cy) + "\"/><wp:docPr id=\"" + idStr +
                    "\" name=\"" + name + "\"/><a:graphic><a:graphicData uri=\"http://schemas.openxmlformats.org/drawingml/2006/picture\">"
                    "<pic:pic><pic:nvPicPr><pic:cNvPr id=\"" + idStr + "\" name=\"" + name +
                    "\"/><pic:cNvPicPr/></pic:nvPicPr><pic:blipFill><a:blip r:embed=\"" + embedId +
                    "\"/><a:stretch><a:fillRect/></a:stretch></pic:blipFill><pic:spPr><a:xfrm><a:off x=\"0\" y=\"0\"/><a:ext cx=\"" +
                    std::to_string(cx) + "\" cy=\"" + std::to_string(cy) +
                    "\"/></a:xfrm><a:prstGeom prst=\"rect\"><a:avLst/></a:prstGeom></pic:spPr></pic:pic></a:graphicData></a:graphic></wp:inline></w:drawing></w:r></w:p>";
    }
    std::string relationships;
    for (size_t i = 0; i < images.size(); ++i) {
        relationships += "<Relationship Id=\"img" + std::to_string(i) +
                         "\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/image\" Target=\"media/image" +
                         std::to_string(i) + "." + images[i].type + "\"/>";
    }
    std::string xml = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>";
    std::vector<std::pair<std::string, std::string>> files;
    files.push_back({
        "[Content_Types].xml",
        xml + "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
              "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
              "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
              "<Default Extension=\"png\" ContentType=\"image/png\"/>"
              "<Default Extension=\"jpeg\" ContentType=\"image/jpeg\"/>"
              "<Default Extension=\"gif\" ContentType=\"image/gif\"/>"
              "<Override PartName=\"/word/document.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>"
              "</Types>"
    });
    files.push_back({
        "_rels/.rels",
        xml + "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
              "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"word/document.xml\"/>"
              "</Relationships>"
    });
    files.push_back({
        "word/_rels/document.xml.rels",
        xml + "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">" + relationships + "</Relationships>"
    });
    files.push_back({
        "word/document.xml",
        xml + "<w:document " + ns + "><w:body>" + body + pictures + "</w:body></w:document>"
    });
    for (size_t i = 0; i < images.size(); ++i) {
        files.push_back({"word/media/image" + std::to_string(i) + "." + images[i].type, images[i].bytes});
    }
    return zip_store(files);
}

// Low-level authenticated fetch helper with 401 retry
inline HttpCallResponse google_api_fetch(
    HttpTransport transport,
    GwsRunner runner,
    const std::string& url,
    const std::string& method = "GET",
    const std::string& body = "",
    const std::map<std::string, std::string>& customHeaders = {},
    const std::string& targetId = ""
) {
    auto fn = transport ? transport : default_curl_transport;
    std::string token = valid_access_token_get(false, transport, runner);

    std::map<std::string, std::string> headers = customHeaders;
    headers["Authorization"] = "Bearer " + token;

    HttpCall req{
        .method = method,
        .url = url,
        .headers = headers,
        .body = body,
        .timeoutMs = 30000,
    };

    HttpCallResponse res = fn(req);

    if (res.status == 401) {
        token = valid_access_token_get(true, transport, runner);
        req.headers["Authorization"] = "Bearer " + token;
        res = fn(req);
    }

    if (res.status >= 400) {
        throw std::runtime_error(gws_error_format(res.body, targetId));
    }

    return res;
}

// Options configuring retry count and backoff timing
struct FetchRetryOptions {
    long backoffMs = 500;
    int retries = 3;
};

// HTTP fetch wrapper providing automatic retries and exponential backoff on transient errors (429, 5xx)
inline HttpCallResponse fetch_with_retry(
    HttpTransport transport,
    GwsRunner runner,
    const std::string& url,
    const std::string& method = "GET",
    const std::string& body = "",
    const std::map<std::string, std::string>& customHeaders = {},
    const std::string& targetId = "",
    const FetchRetryOptions& options = {}
) {
    auto fn = transport ? transport : default_curl_transport;
    long backoff = options.backoffMs;
    int maxAttempts = std::max(1, options.retries);
    std::string lastError;

    for (int attempt = 0; attempt < maxAttempts; ++attempt) {
        try {
            std::string token = valid_access_token_get(false, transport, runner);
            std::map<std::string, std::string> headers = customHeaders;
            headers["Authorization"] = "Bearer " + token;

            HttpCall req{
                .method = method,
                .url = url,
                .headers = headers,
                .body = body,
                .timeoutMs = 30000,
            };

            HttpCallResponse res = fn(req);
            if (res.status == 401) {
                token = valid_access_token_get(true, transport, runner);
                req.headers["Authorization"] = "Bearer " + token;
                res = fn(req);
            }

            // Retry on 429 and 5xx (except 501 Not Implemented)
            if ((res.status == 429 || (res.status >= 500 && res.status != 501)) && attempt < maxAttempts - 1) {
                if (backoff > 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(backoff * (1 << attempt)));
                }
                continue;
            }

            if (res.status >= 400) {
                throw std::runtime_error(gws_error_format(res.body, targetId));
            }

            return res;
        } catch (const std::exception& e) {
            lastError = e.what();
            if (attempt < maxAttempts - 1) {
                std::string msg = e.what();
                bool isTransient = (msg.find("HTTP request failed") != std::string::npos ||
                                    msg.find("ECONNRESET") != std::string::npos ||
                                    msg.find("ETIMEDOUT") != std::string::npos ||
                                    msg.find("socket hang up") != std::string::npos ||
                                    msg.find("fetch failed") != std::string::npos ||
                                    msg.find("timeout") != std::string::npos ||
                                    msg.find("network timeout") != std::string::npos);
                if (isTransient) {
                    if (backoff > 0) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(backoff * (1 << attempt)));
                    }
                    continue;
                }
            }
            throw;
        }
    }
    throw std::runtime_error("Fetch failed after " + std::to_string(maxAttempts) + " attempts: " + url);
}

// Google Docs API client
class DocsClient {
public:
    HttpTransport transport = nullptr;
    GwsRunner runner = nullptr;

    explicit DocsClient(HttpTransport t = nullptr, GwsRunner r = nullptr)
        : transport(t), runner(r) {}

    /// Executes an arbitrary gws CLI command.
    std::string run(const std::vector<std::string>& args) {
        if (runner) return runner(args);
        return exec_gws(args);
    }

    /// getDocument: fetches document AST with full tabs content and inline suggestions (with retry)
    slimr::Json getDocument(const std::string& docId) {
        std::string cleanId = document_id(docId);
        std::string url = "https://docs.googleapis.com/v1/documents/" + detail::url_encode(cleanId) +
                          "?includeTabsContent=true&suggestionsViewMode=SUGGESTIONS_INLINE";

        auto res = fetch_with_retry(transport, runner, url, "GET", "", {}, cleanId, FetchRetryOptions{.retries = 3});
        auto parsed = slimr::parse_json<slimr::Json>(res.body);
        if (!parsed) {
            throw std::runtime_error("Invalid JSON response from Google Docs getDocument");
        }
        return *parsed;
    }

    /// createDocument: creates a new document with title, optionally moving it into folderId
    slimr::Json createDocument(const std::string& title, const std::optional<std::string>& folderId = std::nullopt) {
        std::string url = "https://docs.googleapis.com/v1/documents";
        slimr::Json body = slimr::Json::object_t{{"title", title}};
        std::string bodyStr = slimr::write_json(body);

        auto res = google_api_fetch(transport, runner, url, "POST", bodyStr, {{"Content-Type", "application/json"}});
        auto parsed = slimr::parse_json<slimr::Json>(res.body);
        if (!parsed) {
            throw std::runtime_error("Invalid JSON response from Google Docs createDocument");
        }
        auto docObj = *parsed;
        std::string docId;
        if (docObj.contains("documentId") && docObj["documentId"].is_string()) {
            docId = docObj["documentId"].get<std::string>();
        } else if (docObj.contains("id") && docObj["id"].is_string()) {
            docId = docObj["id"].get<std::string>();
        }

        if (folderId.has_value() && !folderId->empty() && !docId.empty()) {
            std::string driveUrl = "https://www.googleapis.com/drive/v3/files/" + detail::url_encode(docId) +
                                   "?addParents=" + detail::url_encode(*folderId) + "&supportsAllDrives=true";
            google_api_fetch(transport, runner, driveUrl, "PATCH", "{}", {{"Content-Type", "application/json"}}, docId);
        }
        return docObj;
    }

    /// batchUpdate: sends batchUpdate requests with requiredRevisionId. Never retried.
    slimr::Json batchUpdate(
        const std::string& docId,
        const slimr::Json::array_t& requests,
        const std::optional<std::string>& requiredRevisionId = std::nullopt
    ) {
        std::string cleanId = document_id(docId);
        std::string url = "https://docs.googleapis.com/v1/documents/" + detail::url_encode(cleanId) + ":batchUpdate";

        slimr::Json body = slimr::Json::object_t{{"requests", requests}};
        if (requiredRevisionId.has_value() && !requiredRevisionId->empty()) {
            body["writeControl"] = slimr::Json::object_t{{"requiredRevisionId", *requiredRevisionId}};
        }
        std::string bodyStr = slimr::write_json(body);

        auto res = google_api_fetch(transport, runner, url, "POST", bodyStr, {{"Content-Type", "application/json"}}, cleanId);
        auto parsed = slimr::parse_json<slimr::Json>(res.body);
        if (!parsed) {
            throw std::runtime_error("Invalid JSON response from Google Docs batchUpdate");
        }
        return *parsed;
    }
};

// Google Drive API client
class DriveClient {
public:
    HttpTransport transport = nullptr;
    GwsRunner runner = nullptr;

    explicit DriveClient(HttpTransport t = nullptr, GwsRunner r = nullptr)
        : transport(t), runner(r) {}

    /// Executes an arbitrary gws CLI command.
    std::string run(const std::vector<std::string>& args) {
        if (runner) return runner(args);
        return exec_gws(args);
    }

    /// userEmailGet: gets authenticated user's email
    std::string userEmailGet() {
        std::string url = "https://www.googleapis.com/drive/v3/about?fields=user(emailAddress)";
        auto res = google_api_fetch(transport, runner, url);
        auto parsed = slimr::parse_json<slimr::Json>(res.body);
        if (parsed && parsed->contains("user") && (*parsed)["user"].contains("emailAddress") && (*parsed)["user"]["emailAddress"].is_string()) {
            return (*parsed)["user"]["emailAddress"].get<std::string>();
        }
        throw std::runtime_error("Google Drive did not identify the authenticated user");
    }

    /// getFile: fetches file metadata
    slimr::Json getFile(const std::string& fileId, const std::string& fields = "id,name,mimeType,trashed") {
        std::string cleanId = document_id(fileId);
        std::string url = "https://www.googleapis.com/drive/v3/files/" + detail::url_encode(cleanId) +
                          "?supportsAllDrives=true&fields=" + detail::url_encode(fields);
        auto res = google_api_fetch(transport, runner, url, "GET", "", {}, cleanId);
        auto parsed = slimr::parse_json<slimr::Json>(res.body);
        if (!parsed) throw std::runtime_error("Invalid JSON from Drive getFile");
        return *parsed;
    }

    /// getFileMetadata: fetches and parses typed file metadata
    DriveFile getFileMetadata(
        const std::string& fileId,
        const std::string& fields = "id,name,mimeType,trashed,parents,webViewLink,createdTime,modifiedTime,version,size"
    ) {
        auto j = getFile(fileId, fields);
        DriveFile df;
        if (j.contains("id") && j["id"].is_string()) df.id = j["id"].get<std::string>();
        if (j.contains("name") && j["name"].is_string()) df.name = j["name"].get<std::string>();
        if (j.contains("mimeType") && j["mimeType"].is_string()) df.mimeType = j["mimeType"].get<std::string>();
        if (j.contains("trashed") && j["trashed"].is_boolean()) df.trashed = j["trashed"].get<bool>();
        if (j.contains("parents") && j["parents"].is_array()) {
            for (const auto& p : j["parents"].get<slimr::Json::array_t>()) {
                if (p.is_string()) df.parents.push_back(p.get<std::string>());
            }
        }
        if (j.contains("webViewLink") && j["webViewLink"].is_string()) df.webViewLink = j["webViewLink"].get<std::string>();
        if (j.contains("createdTime") && j["createdTime"].is_string()) df.createdTime = j["createdTime"].get<std::string>();
        if (j.contains("modifiedTime") && j["modifiedTime"].is_string()) df.modifiedTime = j["modifiedTime"].get<std::string>();
        if (j.contains("version") && j["version"].is_string()) df.version = j["version"].get<std::string>();
        if (j.contains("size") && j["size"].is_number()) df.size = j["size"].get<int64_t>();
        return df;
    }

    /// listFiles: queries Drive files matching query (paginated)
    std::vector<DriveFile> listFiles(
        const std::string& query = "",
        const std::string& fields = "files(id,name,mimeType,trashed,parents,webViewLink),nextPageToken"
    ) {
        std::string pageToken;
        std::vector<DriveFile> result;
        do {
            std::string q = "supportsAllDrives=true&includeItemsFromAllDrives=true&pageSize=100&fields=" + detail::url_encode(fields);
            if (!query.empty()) {
                q += "&q=" + detail::url_encode(query);
            }
            if (!pageToken.empty()) {
                q += "&pageToken=" + detail::url_encode(pageToken);
            }
            std::string url = "https://www.googleapis.com/drive/v3/files?" + q;
            auto res = google_api_fetch(transport, runner, url, "GET");
            auto parsed = slimr::parse_json<slimr::Json>(res.body);
            if (!parsed) throw std::runtime_error("Invalid JSON from Drive listFiles");

            pageToken = "";
            if (parsed->contains("nextPageToken") && (*parsed)["nextPageToken"].is_string()) {
                pageToken = (*parsed)["nextPageToken"].get<std::string>();
            }

            if (parsed->contains("files") && (*parsed)["files"].is_array()) {
                for (const auto& j : (*parsed)["files"].get<slimr::Json::array_t>()) {
                    DriveFile df;
                    if (j.contains("id") && j["id"].is_string()) df.id = j["id"].get<std::string>();
                    if (j.contains("name") && j["name"].is_string()) df.name = j["name"].get<std::string>();
                    if (j.contains("mimeType") && j["mimeType"].is_string()) df.mimeType = j["mimeType"].get<std::string>();
                    if (j.contains("trashed") && j["trashed"].is_boolean()) df.trashed = j["trashed"].get<bool>();
                    if (j.contains("parents") && j["parents"].is_array()) {
                        for (const auto& p : j["parents"].get<slimr::Json::array_t>()) {
                            if (p.is_string()) df.parents.push_back(p.get<std::string>());
                        }
                    }
                    if (j.contains("webViewLink") && j["webViewLink"].is_string()) df.webViewLink = j["webViewLink"].get<std::string>();
                    result.push_back(std::move(df));
                }
            }
        } while (!pageToken.empty());

        return result;
    }

    /// createFile: creates a new file metadata entry in Drive
    slimr::Json createFile(
        const std::string& name,
        const std::string& mimeType,
        const std::optional<std::string>& folderId = std::nullopt
    ) {
        std::string url = "https://www.googleapis.com/drive/v3/files?supportsAllDrives=true";
        slimr::Json body = slimr::Json::object_t{
            {"name", name},
            {"mimeType", mimeType}
        };
        if (folderId.has_value() && !folderId->empty()) {
            body["parents"] = slimr::Json::array_t{*folderId};
        }
        auto res = google_api_fetch(transport, runner, url, "POST", slimr::write_json(body), {{"Content-Type", "application/json"}});
        auto parsed = slimr::parse_json<slimr::Json>(res.body);
        if (!parsed) throw std::runtime_error("Invalid JSON from Drive createFile");
        return *parsed;
    }

    /// createDocument: creates a new Google Doc, optionally inside folderId
    slimr::Json createDocument(
        const std::string& title,
        const std::optional<std::string>& folderId = std::nullopt
    ) {
        return DocsClient(transport, runner).createDocument(title, folderId);
    }

    /// copyFile: copies a file
    slimr::Json copyFile(const std::string& fileId, const std::string& name) {
        std::string cleanId = document_id(fileId);
        std::string url = "https://www.googleapis.com/drive/v3/files/" + detail::url_encode(cleanId) + "/copy?supportsAllDrives=true";
        slimr::Json body = slimr::Json::object_t{{"name", name}};
        auto res = google_api_fetch(transport, runner, url, "POST", slimr::write_json(body), {{"Content-Type", "application/json"}}, cleanId);
        auto parsed = slimr::parse_json<slimr::Json>(res.body);
        if (!parsed) throw std::runtime_error("Invalid JSON from Drive copyFile");
        return *parsed;
    }

    /// updateFile: updates file metadata (e.g. trashed or rename)
    slimr::Json updateFile(const std::string& fileId, const slimr::Json& body) {
        std::string cleanId = document_id(fileId);
        std::string url = "https://www.googleapis.com/drive/v3/files/" + detail::url_encode(cleanId) + "?supportsAllDrives=true";
        auto res = google_api_fetch(transport, runner, url, "PATCH", slimr::write_json(body), {{"Content-Type", "application/json"}}, cleanId);
        auto parsed = slimr::parse_json<slimr::Json>(res.body);
        if (!parsed) throw std::runtime_error("Invalid JSON from Drive updateFile");
        return *parsed;
    }

    /// deleteFile: permanently deletes a file
    void deleteFile(const std::string& fileId) {
        std::string cleanId = document_id(fileId);
        std::string url = "https://www.googleapis.com/drive/v3/files/" + detail::url_encode(cleanId) + "?supportsAllDrives=true";
        google_api_fetch(transport, runner, url, "DELETE", "", {}, cleanId);
    }

    /// listPermissions: lists permissions on a file
    std::vector<DrivePermission> listPermissions(const std::string& fileId, const std::string& fields = "permissions(id,displayName,emailAddress,domain,role,type)") {
        std::string cleanId = document_id(fileId);
        std::string url = "https://www.googleapis.com/drive/v3/files/" + detail::url_encode(cleanId) +
                          "/permissions?supportsAllDrives=true&pageSize=100&fields=" + detail::url_encode(fields);
        auto res = google_api_fetch(transport, runner, url, "GET", "", {}, cleanId);
        auto parsed = slimr::parse_json<slimr::Json>(res.body);
        std::vector<DrivePermission> result;
        if (parsed && parsed->contains("permissions") && (*parsed)["permissions"].is_array()) {
            for (const auto& item : (*parsed)["permissions"].get<slimr::Json::array_t>()) {
                DrivePermission perm{
                    .id = item.contains("id") && item["id"].is_string() ? item["id"].get<std::string>() : "",
                    .role = item.contains("role") && item["role"].is_string() ? item["role"].get<std::string>() : "",
                    .type = item.contains("type") && item["type"].is_string() ? item["type"].get<std::string>() : "",
                    .displayName = item.contains("displayName") && item["displayName"].is_string() ? std::make_optional(item["displayName"].get<std::string>()) : std::nullopt,
                    .domain = item.contains("domain") && item["domain"].is_string() ? std::make_optional(item["domain"].get<std::string>()) : std::nullopt,
                    .emailAddress = item.contains("emailAddress") && item["emailAddress"].is_string() ? std::make_optional(item["emailAddress"].get<std::string>()) : std::nullopt,
                };
                result.push_back(std::move(perm));
            }
        }
        return result;
    }

    /// createPermission: creates permission on file
    DrivePermission createPermission(
        const std::string& fileId,
        const DrivePermission& permission,
        const std::optional<DrivePermissionCreateOptions>& options = std::nullopt
    ) {
        std::string cleanId = document_id(fileId);
        std::string q = "fields=id,displayName,emailAddress,domain,role,type&supportsAllDrives=true";
        if (options.has_value()) {
            if (options->emailMessage.has_value()) q += "&emailMessage=" + detail::url_encode(*options->emailMessage);
            if (options->moveToNewOwnersRoot.has_value()) q += "&moveToNewOwnersRoot=" + std::string(*options->moveToNewOwnersRoot ? "true" : "false");
            if (options->sendNotificationEmail.has_value()) q += "&sendNotificationEmail=" + std::string(*options->sendNotificationEmail ? "true" : "false");
            if (options->transferOwnership.has_value()) q += "&transferOwnership=" + std::string(*options->transferOwnership ? "true" : "false");
        }
        std::string url = "https://www.googleapis.com/drive/v3/files/" + detail::url_encode(cleanId) + "/permissions?" + q;

        slimr::Json body = slimr::Json::object_t{
            {"role", permission.role},
            {"type", permission.type == "internal" ? "domain" : permission.type},
        };
        if (permission.emailAddress.has_value()) body["emailAddress"] = *permission.emailAddress;
        if (permission.domain.has_value()) body["domain"] = *permission.domain;

        auto res = google_api_fetch(transport, runner, url, "POST", slimr::write_json(body), {{"Content-Type", "application/json"}}, cleanId);
        auto parsed = slimr::parse_json<slimr::Json>(res.body);
        if (!parsed) throw std::runtime_error("Invalid JSON from Drive createPermission");
        const auto& item = *parsed;
        return DrivePermission{
            .id = item.contains("id") && item["id"].is_string() ? item["id"].get<std::string>() : "",
            .role = item.contains("role") && item["role"].is_string() ? item["role"].get<std::string>() : "",
            .type = item.contains("type") && item["type"].is_string() ? item["type"].get<std::string>() : "",
            .displayName = item.contains("displayName") && item["displayName"].is_string() ? std::make_optional(item["displayName"].get<std::string>()) : std::nullopt,
            .domain = item.contains("domain") && item["domain"].is_string() ? std::make_optional(item["domain"].get<std::string>()) : std::nullopt,
            .emailAddress = item.contains("emailAddress") && item["emailAddress"].is_string() ? std::make_optional(item["emailAddress"].get<std::string>()) : std::nullopt,
        };
    }

    /// deletePermission: deletes permission on file
    void deletePermission(const std::string& fileId, const std::string& permissionId) {
        std::string cleanId = document_id(fileId);
        std::string url = "https://www.googleapis.com/drive/v3/files/" + detail::url_encode(cleanId) +
                          "/permissions/" + detail::url_encode(permissionId) + "?supportsAllDrives=true";
        google_api_fetch(transport, runner, url, "DELETE", "", {}, cleanId);
    }

    /// commentsList: paginates and decodes HTML entities in quoted content
    std::vector<DriveComment> commentsList(const std::string& fileId) {
        std::string cleanId = document_id(fileId);
        std::vector<DriveComment> comments;
        std::string pageToken;

        do {
            std::string q = "fields=comments(id,content,author,createdTime,modifiedTime,resolved,deleted,quotedFileContent/value,replies),nextPageToken&includeDeleted=false&pageSize=100";
            if (!pageToken.empty()) {
                q += "&pageToken=" + detail::url_encode(pageToken);
            }
            std::string url = "https://www.googleapis.com/drive/v3/files/" + detail::url_encode(cleanId) + "/comments?" + q;
            auto res = google_api_fetch(transport, runner, url, "GET", "", {}, cleanId);
            auto parsed = slimr::parse_json<slimr::Json>(res.body);
            if (!parsed) throw std::runtime_error("Invalid JSON from commentsList");

            pageToken = "";
            if (parsed->contains("nextPageToken") && (*parsed)["nextPageToken"].is_string()) {
                pageToken = (*parsed)["nextPageToken"].get<std::string>();
            }

            if (parsed->contains("comments") && (*parsed)["comments"].is_array()) {
                for (const auto& item : (*parsed)["comments"].get<slimr::Json::array_t>()) {
                    if (item.contains("deleted") && item["deleted"].is_boolean() && item["deleted"].get<bool>()) {
                        continue;
                    }
                    DriveComment c{
                        .id = item.contains("id") && item["id"].is_string() ? item["id"].get<std::string>() : "",
                        .content = item.contains("content") && item["content"].is_string() ? std::make_optional(item["content"].get<std::string>()) : std::nullopt,
                        .author = std::nullopt,
                        .createdTime = item.contains("createdTime") && item["createdTime"].is_string() ? std::make_optional(item["createdTime"].get<std::string>()) : std::nullopt,
                        .modifiedTime = item.contains("modifiedTime") && item["modifiedTime"].is_string() ? std::make_optional(item["modifiedTime"].get<std::string>()) : std::nullopt,
                        .deleted = item.contains("deleted") && item["deleted"].is_boolean() ? std::make_optional(item["deleted"].get<bool>()) : std::nullopt,
                        .resolved = item.contains("resolved") && item["resolved"].is_boolean() ? std::make_optional(item["resolved"].get<bool>()) : std::nullopt,
                        .quotedText = std::nullopt,
                        .quotedFileContent = std::nullopt,
                        .replies = {},
                    };

                    if (item.contains("author") && item["author"].is_object()) {
                        const auto& a = item["author"];
                        c.author = DriveCommentAuthor{
                            .displayName = a.contains("displayName") && a["displayName"].is_string() ? std::make_optional(a["displayName"].get<std::string>()) : std::nullopt,
                            .emailAddress = a.contains("emailAddress") && a["emailAddress"].is_string() ? std::make_optional(a["emailAddress"].get<std::string>()) : std::nullopt,
                            .me = a.contains("me") && a["me"].is_boolean() ? std::make_optional(a["me"].get<bool>()) : std::nullopt,
                        };
                    }

                    if (item.contains("quotedFileContent") && item["quotedFileContent"].is_object() &&
                        item["quotedFileContent"].contains("value") && item["quotedFileContent"]["value"].is_string()) {
                        std::string decodedQuote = entities_decode(item["quotedFileContent"]["value"].get<std::string>());
                        c.quotedText = decodedQuote;
                        c.quotedFileContent = DriveCommentQuotedContent{.value = decodedQuote};
                    }

                    if (item.contains("replies") && item["replies"].is_array()) {
                        for (const auto& rItem : item["replies"].get<slimr::Json::array_t>()) {
                            DriveCommentReply rep{
                                .id = rItem.contains("id") && rItem["id"].is_string() ? rItem["id"].get<std::string>() : "",
                                .content = rItem.contains("content") && rItem["content"].is_string() ? std::make_optional(rItem["content"].get<std::string>()) : std::nullopt,
                                .deleted = rItem.contains("deleted") && rItem["deleted"].is_boolean() ? std::make_optional(rItem["deleted"].get<bool>()) : std::nullopt,
                                .action = rItem.contains("action") && rItem["action"].is_string() ? std::make_optional(rItem["action"].get<std::string>()) : std::nullopt,
                                .author = std::nullopt,
                                .createdTime = rItem.contains("createdTime") && rItem["createdTime"].is_string() ? std::make_optional(rItem["createdTime"].get<std::string>()) : std::nullopt,
                                .modifiedTime = rItem.contains("modifiedTime") && rItem["modifiedTime"].is_string() ? std::make_optional(rItem["modifiedTime"].get<std::string>()) : std::nullopt,
                            };
                            if (rItem.contains("author") && rItem["author"].is_object()) {
                                const auto& a = rItem["author"];
                                rep.author = DriveCommentAuthor{
                                    .displayName = a.contains("displayName") && a["displayName"].is_string() ? std::make_optional(a["displayName"].get<std::string>()) : std::nullopt,
                                    .emailAddress = a.contains("emailAddress") && a["emailAddress"].is_string() ? std::make_optional(a["emailAddress"].get<std::string>()) : std::nullopt,
                                    .me = a.contains("me") && a["me"].is_boolean() ? std::make_optional(a["me"].get<bool>()) : std::nullopt,
                                };
                            }
                            c.replies.push_back(std::move(rep));
                        }
                    }

                    comments.push_back(std::move(c));
                }
            }
        } while (!pageToken.empty());

        return comments;
    }

    /// listComments: alias for commentsList
    std::vector<DriveComment> listComments(const std::string& fileId) {
        return commentsList(fileId);
    }

    /// Generic comments request helper matching Node.js commentRequest
    slimr::Json commentRequest(
        const std::string& fileId,
        const std::string& suffix,
        const std::string& method,
        const slimr::Json* body = nullptr
    ) {
        std::string cleanId = document_id(fileId);
        std::string url = "https://www.googleapis.com/drive/v3/files/" + detail::url_encode(cleanId) +
                          "/comments" + suffix + (method == "DELETE" ? "" : "?fields=*");
        std::string bodyStr = body ? slimr::write_json(*body) : "";
        std::map<std::string, std::string> headers;
        if (body) {
            headers["Content-Type"] = "application/json";
        }
        auto res = google_api_fetch(transport, runner, url, method, bodyStr, headers, cleanId);
        if (method == "DELETE" || res.body.empty()) {
            return slimr::Json::object_t{};
        }
        auto parsed = slimr::parse_json<slimr::Json>(res.body);
        if (!parsed) throw std::runtime_error("Invalid JSON from commentRequest");
        return *parsed;
    }

    /// commentGet: reads one comment thread
    slimr::Json commentGet(const std::string& fileId, const std::string& threadId) {
        return commentRequest(fileId, "/" + detail::url_encode(threadId), "GET");
    }

    /// getComment: alias for commentGet
    slimr::Json getComment(const std::string& fileId, const std::string& threadId) {
        return commentGet(fileId, threadId);
    }

    /// commentCreate: creates comment thread
    slimr::Json commentCreate(const std::string& fileId, const std::string& text, const std::optional<std::string>& quote = std::nullopt) {
        slimr::Json body = slimr::Json::object_t{{"content", text}};
        if (quote.has_value() && !quote->empty()) {
            body["quotedFileContent"] = slimr::Json::object_t{
                {"mimeType", "text/html"},
                {"value", *quote},
            };
        }
        return commentRequest(fileId, "", "POST", &body);
    }

    /// createComment: alias for commentCreate
    slimr::Json createComment(const std::string& fileId, const std::string& text, const std::optional<std::string>& quote = std::nullopt) {
        return commentCreate(fileId, text, quote);
    }

    /// commentReply: replies, resolves, or reopens comment
    slimr::Json commentReply(
        const std::string& fileId,
        const std::string& threadId,
        const std::optional<std::string>& content,
        const std::optional<std::string>& action = std::nullopt
    ) {
        slimr::Json body = slimr::Json::object_t{};
        if (content.has_value() && !content->empty()) body["content"] = *content;
        if (action.has_value() && !action->empty()) body["action"] = *action;
        return commentRequest(fileId, "/" + detail::url_encode(threadId) + "/replies", "POST", &body);
    }

    /// createReply: alias for commentReply
    slimr::Json createReply(
        const std::string& fileId,
        const std::string& threadId,
        const std::optional<std::string>& content,
        const std::optional<std::string>& action = std::nullopt
    ) {
        return commentReply(fileId, threadId, content, action);
    }

    /// commentUpdate: updates author's comment
    slimr::Json commentUpdate(const std::string& fileId, const std::string& threadId, const std::string& text) {
        slimr::Json body = slimr::Json::object_t{{"content", text}};
        return commentRequest(fileId, "/" + detail::url_encode(threadId), "PATCH", &body);
    }

    /// updateComment: alias for commentUpdate
    slimr::Json updateComment(const std::string& fileId, const std::string& threadId, const std::string& text) {
        return commentUpdate(fileId, threadId, text);
    }

    /// commentDelete: deletes comment thread
    void commentDelete(const std::string& fileId, const std::string& threadId) {
        commentRequest(fileId, "/" + detail::url_encode(threadId), "DELETE");
    }

    /// deleteComment: alias for commentDelete
    void deleteComment(const std::string& fileId, const std::string& threadId) {
        commentDelete(fileId, threadId);
    }

    /// replyUpdate: edits the author's reply
    slimr::Json replyUpdate(const std::string& fileId, const std::string& threadId, const std::string& replyId, const std::string& text) {
        slimr::Json body = slimr::Json::object_t{{"content", text}};
        return commentRequest(fileId, "/" + detail::url_encode(threadId) + "/replies/" + detail::url_encode(replyId), "PATCH", &body);
    }

    /// updateReply: alias for replyUpdate
    slimr::Json updateReply(const std::string& fileId, const std::string& threadId, const std::string& replyId, const std::string& text) {
        return replyUpdate(fileId, threadId, replyId, text);
    }

    /// replyDelete: deletes the author's reply
    void replyDelete(const std::string& fileId, const std::string& threadId, const std::string& replyId) {
        commentRequest(fileId, "/" + detail::url_encode(threadId) + "/replies/" + detail::url_encode(replyId), "DELETE");
    }

    /// deleteReply: alias for replyDelete
    void deleteReply(const std::string& fileId, const std::string& threadId, const std::string& replyId) {
        replyDelete(fileId, threadId, replyId);
    }

    /// imagesUpload: uploads images through a throwaway converted .docx and returns document ID and contentUris
    ImagesUploadResult imagesUpload(const std::string& name, const std::vector<DocxImage>& images) {
        std::string boundary = "gdocsmith-upload";
        slimr::Json meta = slimr::Json::object_t{
            {"mimeType", "application/vnd.google-apps.document"},
            {"name", name}
        };
        std::string metaStr = slimr::write_json(meta);

        std::string docxData = docx_build("", images);

        std::string body;
        body += "--" + boundary + "\r\n";
        body += "Content-Type: application/json\r\n\r\n";
        body += metaStr + "\r\n";
        body += "--" + boundary + "\r\n";
        body += "Content-Type: application/vnd.openxmlformats-officedocument.wordprocessingml.document\r\n\r\n";
        body += docxData;
        body += "\r\n--" + boundary + "--";

        std::string uploadUrl = "https://www.googleapis.com/upload/drive/v3/files?uploadType=multipart&supportsAllDrives=true";
        std::map<std::string, std::string> headers = {
            {"Content-Type", "multipart/related; boundary=" + boundary}
        };

        auto res = google_api_fetch(transport, runner, uploadUrl, "POST", body, headers);
        auto parsed = slimr::parse_json<slimr::Json>(res.body);
        if (!parsed || !parsed->contains("id") || !(*parsed)["id"].is_string()) {
            throw std::runtime_error("Invalid response from Drive imagesUpload");
        }
        std::string documentId = (*parsed)["id"].get<std::string>();

        // Read back document
        std::string readUrl = "https://docs.googleapis.com/v1/documents/" + detail::url_encode(documentId) + "?includeTabsContent=true";
        HttpCallResponse readRes;
        try {
            readRes = google_api_fetch(transport, runner, readUrl, "GET", "", {}, documentId);
        } catch (...) {
            throw;
        }

        auto readParsed = slimr::parse_json<slimr::Json>(readRes.body);
        if (!readParsed) {
            throw std::runtime_error("Invalid JSON reading uploaded document: " + documentId);
        }

        std::vector<std::string> contentUris;
        const auto& docJson = *readParsed;
        slimr::Json documentTab = slimr::Json::object_t{};
        if (docJson.contains("tabs") && docJson["tabs"].is_array() && !docJson["tabs"].get<slimr::Json::array_t>().empty()) {
            const auto& tab0 = docJson["tabs"].get<slimr::Json::array_t>()[0];
            if (tab0.contains("documentTab") && tab0["documentTab"].is_object()) {
                documentTab = tab0["documentTab"];
            }
        }

        const slimr::Json* inlineObjectsPtr = nullptr;
        if (documentTab.is_object() && documentTab.contains("inlineObjects") && documentTab["inlineObjects"].is_object()) {
            inlineObjectsPtr = &documentTab["inlineObjects"];
        } else if (docJson.contains("inlineObjects") && docJson["inlineObjects"].is_object()) {
            inlineObjectsPtr = &docJson["inlineObjects"];
        }

        const slimr::Json* bodyContentPtr = nullptr;
        if (documentTab.is_object() && documentTab.contains("body") && documentTab["body"].is_object() &&
            documentTab["body"].contains("content") && documentTab["body"]["content"].is_array()) {
            bodyContentPtr = &documentTab["body"]["content"];
        } else if (docJson.contains("body") && docJson["body"].is_object() &&
                   docJson["body"].contains("content") && docJson["body"]["content"].is_array()) {
            bodyContentPtr = &docJson["body"]["content"];
        }

        if (bodyContentPtr) {
            for (const auto& el : bodyContentPtr->get<slimr::Json::array_t>()) {
                if (el.contains("paragraph") && el["paragraph"].is_object() &&
                    el["paragraph"].contains("elements") && el["paragraph"]["elements"].is_array()) {
                    for (const auto& pel : el["paragraph"]["elements"].get<slimr::Json::array_t>()) {
                        if (pel.contains("inlineObjectElement") && pel["inlineObjectElement"].is_object() &&
                            pel["inlineObjectElement"].contains("inlineObjectId") && pel["inlineObjectElement"]["inlineObjectId"].is_string()) {
                            std::string objId = pel["inlineObjectElement"]["inlineObjectId"].get<std::string>();
                            std::string uri;
                            if (inlineObjectsPtr && inlineObjectsPtr->contains(objId) && (*inlineObjectsPtr)[objId].is_object()) {
                                const auto& obj = (*inlineObjectsPtr)[objId];
                                if (obj.contains("inlineObjectProperties") && obj["inlineObjectProperties"].is_object()) {
                                    const auto& iop = obj["inlineObjectProperties"];
                                    if (iop.contains("embeddedObject") && iop["embeddedObject"].is_object()) {
                                        const auto& eo = iop["embeddedObject"];
                                        if (eo.contains("imageProperties") && eo["imageProperties"].is_object()) {
                                            const auto& ip = eo["imageProperties"];
                                            if (ip.contains("contentUri") && ip["contentUri"].is_string()) {
                                                uri = ip["contentUri"].get<std::string>();
                                            }
                                        }
                                    }
                                }
                            }
                            if (!uri.empty()) {
                                contentUris.push_back(uri);
                            }
                        }
                    }
                }
            }
        }

        if (contentUris.size() != images.size() || std::any_of(contentUris.begin(), contentUris.end(), [](const std::string& u) { return u.empty(); })) {
            throw std::runtime_error("Google didn't convert every uploaded image");
        }

        return ImagesUploadResult{
            .contentUris = std::move(contentUris),
            .documentId = std::move(documentId)
        };
    }

    /// textExport: plain text export stripped of BOM and \r\n normalized to \n
    std::string textExport(const std::string& documentId) {
        std::string cleanId = document_id(documentId);
        std::string url = "https://www.googleapis.com/drive/v3/files/" + detail::url_encode(cleanId) + "/export?mimeType=text%2Fplain";
        auto res = google_api_fetch(transport, runner, url, "GET", "", {}, cleanId);
        std::string text = std::move(res.body);

        // Strip UTF-8 BOM if present
        if (text.starts_with("\xEF\xBB\xBF")) {
            text.erase(0, 3);
        }

        // Normalize \r\n to \n
        std::string normalized;
        normalized.reserve(text.size());
        for (size_t i = 0; i < text.size(); ++i) {
            if (text[i] == '\r') {
                if (i + 1 < text.size() && text[i + 1] == '\n') {
                    normalized += '\n';
                    i++;
                } else {
                    normalized += '\n';
                }
            } else {
                normalized += text[i];
            }
        }
        return normalized;
    }
};

// Tracks write attempts and outcomes across operations
struct WriteAttempt {
    std::string action;
    std::optional<std::string> doc;
    std::string outcome = "pending"; // "pending", "confirmed", "rejected", "unknown"
    int requestCount = 1;
    std::optional<std::string> revision;
    std::optional<std::string> createdId;
};

class WriteTracker {
public:
    std::vector<WriteAttempt> writes;
    std::map<std::string, std::string> revisions;

    std::string writeState(bool complete) const {
        for (const auto& w : writes) {
            if (w.outcome == "unknown" || w.outcome == "pending") return "unknown";
        }
        bool anyConfirmed = false;
        for (const auto& w : writes) {
            if (w.outcome == "confirmed") {
                anyConfirmed = true;
                break;
            }
        }
        if (!anyConfirmed) return "none";
        return complete ? "complete" : "partial";
    }

    void recordWrite(WriteAttempt attempt) {
        if (attempt.doc.has_value() && attempt.revision.has_value()) {
            revisions[*attempt.doc] = *attempt.revision;
        }
        writes.push_back(std::move(attempt));
    }
};

} // namespace gdocsmith
