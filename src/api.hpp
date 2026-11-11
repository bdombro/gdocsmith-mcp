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

struct DriveComment {
    std::string id;
    std::optional<std::string> content;
    std::optional<DriveCommentAuthor> author;
    std::optional<std::string> createdTime;
    std::optional<std::string> modifiedTime;
    std::optional<bool> deleted;
    std::optional<bool> resolved;
    std::optional<std::string> quotedText;
    std::vector<DriveCommentReply> replies;
};

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

// Google Docs API client
class DocsClient {
public:
    HttpTransport transport = nullptr;
    GwsRunner runner = nullptr;

    explicit DocsClient(HttpTransport t = nullptr, GwsRunner r = nullptr)
        : transport(t), runner(r) {}

    /// getDocument: fetches document AST with full tabs content and inline suggestions
    slimr::Json getDocument(const std::string& docId) {
        std::string cleanId = document_id(docId);
        std::string url = "https://docs.googleapis.com/v1/documents/" + detail::url_encode(cleanId) +
                          "?includeTabsContent=true&suggestionsViewMode=SUGGESTIONS_INLINE";

        auto res = google_api_fetch(transport, runner, url, "GET", "", {}, cleanId);
        auto parsed = slimr::parse_json<slimr::Json>(res.body);
        if (!parsed) {
            throw std::runtime_error("Invalid JSON response from Google Docs getDocument");
        }
        return *parsed;
    }

    /// createDocument: creates a new document with title
    slimr::Json createDocument(const std::string& title) {
        std::string url = "https://docs.googleapis.com/v1/documents";
        slimr::Json body = slimr::Json::object_t{{"title", title}};
        std::string bodyStr = slimr::write_json(body);

        auto res = google_api_fetch(transport, runner, url, "POST", bodyStr, {{"Content-Type", "application/json"}});
        auto parsed = slimr::parse_json<slimr::Json>(res.body);
        if (!parsed) {
            throw std::runtime_error("Invalid JSON response from Google Docs createDocument");
        }
        return *parsed;
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
                        c.quotedText = entities_decode(item["quotedFileContent"]["value"].get<std::string>());
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

    /// commentGet: reads one comment thread
    slimr::Json commentGet(const std::string& fileId, const std::string& threadId) {
        std::string cleanId = document_id(fileId);
        std::string url = "https://www.googleapis.com/drive/v3/files/" + detail::url_encode(cleanId) +
                          "/comments/" + detail::url_encode(threadId) + "?fields=*";
        auto res = google_api_fetch(transport, runner, url, "GET", "", {}, cleanId);
        auto parsed = slimr::parse_json<slimr::Json>(res.body);
        if (!parsed) throw std::runtime_error("Invalid JSON from commentGet");
        return *parsed;
    }

    /// commentCreate: creates comment thread
    slimr::Json commentCreate(const std::string& fileId, const std::string& text, const std::optional<std::string>& quote = std::nullopt) {
        std::string cleanId = document_id(fileId);
        std::string url = "https://www.googleapis.com/drive/v3/files/" + detail::url_encode(cleanId) + "/comments?fields=*";
        slimr::Json body = slimr::Json::object_t{{"content", text}};
        if (quote.has_value() && !quote->empty()) {
            body["quotedFileContent"] = slimr::Json::object_t{
                {"mimeType", "text/html"},
                {"value", *quote},
            };
        }
        auto res = google_api_fetch(transport, runner, url, "POST", slimr::write_json(body), {{"Content-Type", "application/json"}}, cleanId);
        auto parsed = slimr::parse_json<slimr::Json>(res.body);
        if (!parsed) throw std::runtime_error("Invalid JSON from commentCreate");
        return *parsed;
    }

    /// commentReply: replies, resolves, or reopens comment
    slimr::Json commentReply(const std::string& fileId, const std::string& threadId, const std::optional<std::string>& content, const std::optional<std::string>& action) {
        std::string cleanId = document_id(fileId);
        std::string url = "https://www.googleapis.com/drive/v3/files/" + detail::url_encode(cleanId) +
                          "/comments/" + detail::url_encode(threadId) + "/replies?fields=*";
        slimr::Json body = slimr::Json::object_t{};
        if (content.has_value() && !content->empty()) body["content"] = *content;
        if (action.has_value() && !action->empty()) body["action"] = *action;

        auto res = google_api_fetch(transport, runner, url, "POST", slimr::write_json(body), {{"Content-Type", "application/json"}}, cleanId);
        auto parsed = slimr::parse_json<slimr::Json>(res.body);
        if (!parsed) throw std::runtime_error("Invalid JSON from commentReply");
        return *parsed;
    }

    /// commentUpdate: updates author's comment
    slimr::Json commentUpdate(const std::string& fileId, const std::string& threadId, const std::string& text) {
        std::string cleanId = document_id(fileId);
        std::string url = "https://www.googleapis.com/drive/v3/files/" + detail::url_encode(cleanId) +
                          "/comments/" + detail::url_encode(threadId) + "?fields=*";
        slimr::Json body = slimr::Json::object_t{{"content", text}};
        auto res = google_api_fetch(transport, runner, url, "PATCH", slimr::write_json(body), {{"Content-Type", "application/json"}}, cleanId);
        auto parsed = slimr::parse_json<slimr::Json>(res.body);
        if (!parsed) throw std::runtime_error("Invalid JSON from commentUpdate");
        return *parsed;
    }

    /// commentDelete: deletes comment thread
    void commentDelete(const std::string& fileId, const std::string& threadId) {
        std::string cleanId = document_id(fileId);
        std::string url = "https://www.googleapis.com/drive/v3/files/" + detail::url_encode(cleanId) +
                          "/comments/" + detail::url_encode(threadId);
        google_api_fetch(transport, runner, url, "DELETE", "", {}, cleanId);
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
