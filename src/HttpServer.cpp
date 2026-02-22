#include "HttpServer.h"
#include <cstring>
#include <sstream>
#include <iostream>
#include <algorithm>

// ── Cross-platform socket abstraction ────────────────────────────────────────
#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  // Link against Ws2_32.lib automatically via pragma
  #pragma comment(lib, "Ws2_32.lib")

  // On Windows, socket handles are SOCKET (uintptr_t), not int.
  // We alias them so the rest of the code is identical.
  using SocketFd = SOCKET;
  static const SocketFd INVALID_FD = INVALID_SOCKET;

  inline void closeSocket(SocketFd fd) { closesocket(fd); }
  inline bool socketError(SocketFd fd) { return fd == INVALID_SOCKET; }
  // ssize_t doesn't exist on MSVC
  using ssize_t = int;

  // Winsock startup helper (called once in start())
  static bool wsaInit() {
      WSADATA wsa;
      return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
  }
  static void wsaCleanup() { WSACleanup(); }

#else
  // POSIX (Linux, macOS)
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <fcntl.h>

  using SocketFd = int;
  static const SocketFd INVALID_FD = -1;

  inline void closeSocket(SocketFd fd) { close(fd); }
  inline bool socketError(SocketFd fd) { return fd < 0; }
  static bool wsaInit()    { return true; }  // no-op on POSIX
  static void wsaCleanup() {}                // no-op on POSIX
#endif
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
//  Constructor
// ─────────────────────────────────────────────────────────────────────────────

HttpServer::HttpServer(MorphologyEngine& engine, int port)
    : engine_(engine), port_(port) {}

// ─────────────────────────────────────────────────────────────────────────────
//  JSON Helpers  (no external library)
// ─────────────────────────────────────────────────────────────────────────────

std::string HttpServer::jsonStr(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if      (c == '"')  out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else                out += c;
    }
    out += '"';
    return out;
}

std::string HttpServer::jsonField(const std::string& k, const std::string& v) {
    return jsonStr(k) + ":" + jsonStr(v);
}

std::string HttpServer::jsonField(const std::string& k, int v) {
    return jsonStr(k) + ":" + std::to_string(v);
}

std::string HttpServer::jsonField(const std::string& k, bool v) {
    return jsonStr(k) + ":" + (v ? "true" : "false");
}

std::string HttpServer::jsonObject(const std::string& fields) {
    return "{" + fields + "}";
}

std::string HttpServer::jsonArray(const std::vector<std::string>& items) {
    std::string out = "[";
    for (size_t i = 0; i < items.size(); i++) {
        if (i) out += ",";
        out += items[i];
    }
    out += "]";
    return out;
}

/** Tiny JSON value extractor (for flat JSON only) */
std::string HttpServer::getJsonValue(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return "";
    // Skip whitespace
    pos++;
    while (pos < json.size() && json[pos] == ' ') pos++;
    if (pos >= json.size()) return "";
    if (json[pos] == '"') {
        // String value
        pos++;
        std::string val;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.size()) {
                pos++;
                if      (json[pos] == '"')  val += '"';
                else if (json[pos] == '\\') val += '\\';
                else if (json[pos] == 'n')  val += '\n';
                else                        val += json[pos];
            } else {
                val += json[pos];
            }
            pos++;
        }
        return val;
    }
    // Non-string (number / bool)
    std::string val;
    while (pos < json.size() && json[pos] != ',' && json[pos] != '}') {
        val += json[pos++];
    }
    // trim
    while (!val.empty() && val.back() == ' ') val.pop_back();
    return val;
}

// ─────────────────────────────────────────────────────────────────────────────
//  HTTP parsing
// ─────────────────────────────────────────────────────────────────────────────

HttpServer::Request HttpServer::parseRequest(const std::string& raw) {
    Request req;
    std::istringstream ss(raw);
    std::string line;

    // Request line: METHOD PATH HTTP/1.x
    if (std::getline(ss, line)) {
        std::istringstream rl(line);
        std::string proto;
        rl >> req.method >> req.path >> proto;
        // Strip query string
        auto q = req.path.find('?');
        if (q != std::string::npos) req.path = req.path.substr(0, q);
    }

    // Headers
    while (std::getline(ss, line) && line != "\r" && !line.empty()) {
        auto col = line.find(':');
        if (col != std::string::npos) {
            std::string k = line.substr(0, col);
            std::string v = line.substr(col + 1);
            // Trim
            while (!v.empty() && (v[0] == ' ' || v[0] == '\t')) v.erase(0, 1);
            while (!v.empty() && (v.back() == '\r' || v.back() == '\n')) v.pop_back();
            // Lowercase key
            std::transform(k.begin(), k.end(), k.begin(), ::tolower);
            req.headers[k] = v;
        }
    }

    // Body: skip blank line (already consumed via getline loop ending on \r)
    // Find body after double CRLF
    std::string doubleNewline = "\r\n\r\n";
    auto bodyStart = raw.find(doubleNewline);
    if (bodyStart == std::string::npos) {
        doubleNewline = "\n\n";
        bodyStart = raw.find(doubleNewline);
    }
    if (bodyStart != std::string::npos) {
        req.body = raw.substr(bodyStart + doubleNewline.size());
    }
    return req;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Response builder
// ─────────────────────────────────────────────────────────────────────────────

std::string HttpServer::buildResponse(const Response& res) const {
    std::string statusText;
    switch (res.status) {
        case 200: statusText = "OK"; break;
        case 400: statusText = "Bad Request"; break;
        case 404: statusText = "Not Found"; break;
        case 500: statusText = "Internal Server Error"; break;
        default:  statusText = "OK"; break;
    }

    std::ostringstream out;
    out << "HTTP/1.1 " << res.status << " " << statusText << "\r\n";
    out << "Content-Type: " << res.contentType << "; charset=utf-8\r\n";
    out << "Content-Length: " << res.body.size() << "\r\n";
    out << "Access-Control-Allow-Origin: *\r\n";
    out << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    out << "Access-Control-Allow-Headers: Content-Type\r\n";
    out << "Connection: close\r\n";
    out << "\r\n";
    out << res.body;
    return out.str();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Router
// ─────────────────────────────────────────────────────────────────────────────

HttpServer::Response HttpServer::route(const Request& req) {
    // Handle CORS preflight
    if (req.method == "OPTIONS") {
        Response res;
        res.status = 200;
        res.body = "";
        return res;
    }

    if (req.method == "GET") {
        if (req.path == "/" || req.path == "/index.html") {
            Response res;
            res.status = 200;
            res.contentType = "text/html";
            res.body = getHtmlUI();
            return res;
        }
        if (req.path == "/api/roots")    return handleGetRoots();
        if (req.path == "/api/patterns") return handleGetPatterns();
        if (req.path == "/api/stats")    return handleGetStats();
    }

    if (req.method == "POST") {
        if (req.path == "/api/insert-root")    return handleInsertRoot(req.body);
        if (req.path == "/api/remove-root")    return handleRemoveRoot(req.body);
        if (req.path == "/api/search-root")    return handleSearchRoot(req.body);
        if (req.path == "/api/generate")       return handleGenerate(req.body);
        if (req.path == "/api/generate-all")   return handleGenerateAll(req.body);
        if (req.path == "/api/validate")       return handleValidate(req.body);
        if (req.path == "/api/derivatives")    return handleDerivatives(req.body);
        if (req.path == "/api/add-pattern")    return handleAddPattern(req.body);
        if (req.path == "/api/remove-pattern") return handleRemovePattern(req.body);
    }

    Response res;
    res.status = 404;
    res.body = jsonObject(jsonField("error", "Not found"));
    return res;
}

// ─────────────────────────────────────────────────────────────────────────────
//  API Handlers
// ─────────────────────────────────────────────────────────────────────────────

HttpServer::Response HttpServer::handleGetRoots() {
    auto roots = engine_.getAllRoots();
    std::vector<std::string> items;
    for (const auto& r : roots) items.push_back(jsonStr(r));
    Response res;
    res.body = jsonObject(
        jsonField("count", (int)roots.size()) + "," +
        "\"roots\":" + jsonArray(items)
    );
    return res;
}

HttpServer::Response HttpServer::handleGetPatterns() {
    auto patterns = engine_.getAllPatterns();
    std::vector<std::string> items;
    for (const auto& p : patterns) {
        items.push_back(jsonObject(
            jsonField("name", p.name) + "," +
            jsonField("description", p.description) + "," +
            jsonField("category", p.category)
        ));
    }
    Response res;
    res.body = jsonObject(
        jsonField("count", (int)patterns.size()) + "," +
        "\"patterns\":" + jsonArray(items)
    );
    return res;
}

HttpServer::Response HttpServer::handleGetStats() {
    Response res;
    res.body = jsonObject(
        jsonField("rootCount", engine_.rootCount()) + "," +
        jsonField("patternCount", engine_.patternCount())
    );
    return res;
}

HttpServer::Response HttpServer::handleInsertRoot(const std::string& body) {
    std::string root = getJsonValue(body, "root");
    Response res;
    if (root.empty()) {
        res.status = 400;
        res.body = jsonObject(jsonField("error", "Missing 'root' field"));
        return res;
    }
    bool inserted = engine_.insertRoot(root);
    res.body = jsonObject(
        jsonField("success", true) + "," +
        jsonField("inserted", inserted) + "," +
        jsonField("root", root) + "," +
        jsonField("message", inserted ? "Root inserted" : "Root already exists")
    );
    return res;
}

HttpServer::Response HttpServer::handleRemoveRoot(const std::string& body) {
    std::string root = getJsonValue(body, "root");
    Response res;
    if (root.empty()) {
        res.status = 400;
        res.body = jsonObject(jsonField("error", "Missing 'root' field"));
        return res;
    }
    bool removed = engine_.removeRoot(root);
    res.body = jsonObject(
        jsonField("success", removed) + "," +
        jsonField("root", root) + "," +
        jsonField("message", removed ? "Root removed" : "Root not found")
    );
    return res;
}

HttpServer::Response HttpServer::handleSearchRoot(const std::string& body) {
    std::string root = getJsonValue(body, "root");
    Response res;
    if (root.empty()) {
        res.status = 400;
        res.body = jsonObject(jsonField("error", "Missing 'root' field"));
        return res;
    }
    bool found = engine_.searchRoot(root);
    res.body = jsonObject(
        jsonField("found", found) + "," +
        jsonField("root", root) + "," +
        jsonField("message", found ? "Root found in AVL tree" : "Root not found")
    );
    return res;
}

HttpServer::Response HttpServer::handleGenerate(const std::string& body) {
    std::string root    = getJsonValue(body, "root");
    std::string pattern = getJsonValue(body, "pattern");
    Response res;
    if (root.empty() || pattern.empty()) {
        res.status = 400;
        res.body = jsonObject(jsonField("error", "Missing 'root' or 'pattern' field"));
        return res;
    }
    auto result = engine_.generateWord(root, pattern);
    res.body = jsonObject(
        jsonField("success",     result.success) + "," +
        jsonField("root",        result.root) + "," +
        jsonField("pattern",     result.pattern) + "," +
        jsonField("description", result.patternDescription) + "," +
        jsonField("category",    result.patternCategory) + "," +
        jsonField("derivedWord", result.derivedWord) + "," +
        jsonField("error",       result.errorMessage)
    );
    return res;
}

HttpServer::Response HttpServer::handleGenerateAll(const std::string& body) {
    std::string root = getJsonValue(body, "root");
    Response res;
    if (root.empty()) {
        res.status = 400;
        res.body = jsonObject(jsonField("error", "Missing 'root' field"));
        return res;
    }
    auto results = engine_.generateAllDerivatives(root);
    std::vector<std::string> items;
    for (const auto& r : results) {
        items.push_back(jsonObject(
            jsonField("pattern",     r.pattern) + "," +
            jsonField("description", r.patternDescription) + "," +
            jsonField("category",    r.patternCategory) + "," +
            jsonField("derivedWord", r.derivedWord)
        ));
    }
    res.body = jsonObject(
        jsonField("root", root) + "," +
        jsonField("count", (int)results.size()) + "," +
        "\"derivatives\":" + jsonArray(items)
    );
    return res;
}

HttpServer::Response HttpServer::handleValidate(const std::string& body) {
    std::string word = getJsonValue(body, "word");
    std::string root = getJsonValue(body, "root");
    Response res;
    if (word.empty() || root.empty()) {
        res.status = 400;
        res.body = jsonObject(jsonField("error", "Missing 'word' or 'root' field"));
        return res;
    }
    auto result = engine_.validateWord(word, root);
    res.body = jsonObject(
        jsonField("isValid",      result.isValid) + "," +
        jsonField("word",         word) + "," +
        jsonField("root",         root) + "," +
        jsonField("matchedPattern",     result.matchedPattern) + "," +
        jsonField("patternDescription", result.patternDescription) + "," +
        jsonField("message",      result.message)
    );
    return res;
}

HttpServer::Response HttpServer::handleDerivatives(const std::string& body) {
    std::string root = getJsonValue(body, "root");
    Response res;
    if (root.empty()) {
        res.status = 400;
        res.body = jsonObject(jsonField("error", "Missing 'root' field"));
        return res;
    }
    auto derivs = engine_.getDerivatives(root);
    std::vector<std::string> items;
    for (const auto& d : derivs) {
        items.push_back(jsonObject(
            jsonField("word",      d.word) + "," +
            jsonField("pattern",   d.pattern) + "," +
            jsonField("frequency", d.frequency)
        ));
    }
    res.body = jsonObject(
        jsonField("root", root) + "," +
        jsonField("count", (int)derivs.size()) + "," +
        "\"derivatives\":" + jsonArray(items)
    );
    return res;
}

HttpServer::Response HttpServer::handleAddPattern(const std::string& body) {
    std::string name = getJsonValue(body, "name");
    std::string desc = getJsonValue(body, "desc");
    std::string cat  = getJsonValue(body, "cat");
    Response res;
    if (name.empty()) {
        res.status = 400;
        res.body = jsonObject(jsonField("error", "Missing 'name' field"));
        return res;
    }
    bool added = engine_.addPattern(name, desc, cat);
    res.body = jsonObject(
        jsonField("success", added) + "," +
        jsonField("name", name) + "," +
        jsonField("message", added ? "Pattern added" : "Pattern already exists")
    );
    return res;
}

HttpServer::Response HttpServer::handleRemovePattern(const std::string& body) {
    std::string name = getJsonValue(body, "name");
    Response res;
    if (name.empty()) {
        res.status = 400;
        res.body = jsonObject(jsonField("error", "Missing 'name' field"));
        return res;
    }
    bool removed = engine_.removePattern(name);
    res.body = jsonObject(
        jsonField("success", removed) + "," +
        jsonField("name", name) + "," +
        jsonField("message", removed ? "Pattern removed" : "Pattern not found")
    );
    return res;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Main server loop  (Windows Winsock2 + POSIX compatible)
// ─────────────────────────────────────────────────────────────────────────────

void HttpServer::start() {
    // Initialize Winsock on Windows (no-op on Linux/Mac)
    if (!wsaInit()) {
        std::cerr << "Failed to initialise Winsock\n";
        return;
    }

    SocketFd serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketError(serverFd)) {
        std::cerr << "Failed to create socket\n";
        wsaCleanup();
        return;
    }

    // Allow address reuse so we can restart quickly
    int opt = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(static_cast<u_short>(port_));

    if (bind(serverFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        std::cerr << "Bind failed on port " << port_
                  << " — is another process using it?\n";
        closeSocket(serverFd);
        wsaCleanup();
        return;
    }

    if (listen(serverFd, 10) != 0) {
        std::cerr << "Listen failed\n";
        closeSocket(serverFd);
        wsaCleanup();
        return;
    }

    std::cout << "\n+==================================================+\n";
    std::cout << "|   Arabic Morphological Engine - HTTP Server      |\n";
    std::cout << "|   http://localhost:" << port_
              << "                             |\n";
    std::cout << "+==================================================+\n\n";
    std::cout << "Engine loaded: "
              << engine_.rootCount() << " roots, "
              << engine_.patternCount() << " patterns\n";
    std::cout << "Open your browser at http://localhost:" << port_ << "\n";
    std::cout << "Press Ctrl+C to stop.\n\n";

    while (true) {
        sockaddr_in clientAddr{};
#ifdef _WIN32
        int clientLen = sizeof(clientAddr);
#else
        socklen_t clientLen = sizeof(clientAddr);
#endif
        SocketFd clientFd = accept(serverFd,
                                   reinterpret_cast<sockaddr*>(&clientAddr),
                                   &clientLen);
        if (socketError(clientFd)) continue;

        // Read request (up to 64 KB)
        char buf[65536] = {};
        ssize_t bytesRead = recv(clientFd, buf, sizeof(buf) - 1, 0);
        if (bytesRead > 0) {
            std::string rawReq(buf, static_cast<size_t>(bytesRead));
            Request  req  = parseRequest(rawReq);
            Response resp = route(req);
            std::string http = buildResponse(resp);
            send(clientFd, http.c_str(),
                 static_cast<int>(http.size()), 0);

            std::cout << "[" << req.method << "] " << req.path
                      << " -> " << resp.status << "\n";
        }
        closeSocket(clientFd);
    }

    closeSocket(serverFd);
    wsaCleanup();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Embedded HTML UI  (returned for GET /)
// ─────────────────────────────────────────────────────────────────────────────

std::string HttpServer::getHtmlUI() const {
    // The full HTML is embedded as a raw string literal.
    // It references the /api/* endpoints above.
    return R"HTML(<!DOCTYPE html>
<html lang="ar" dir="rtl">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>محرك البحث الصرفي العربي</title>
<link href="https://fonts.googleapis.com/css2?family=Amiri:ital,wght@0,400;0,700;1,400&family=Noto+Naskh+Arabic:wght@400;500;600;700&family=IBM+Plex+Mono&display=swap" rel="stylesheet">
<style>
:root {
  --bg:        #0d0f14;
  --surface:   #131720;
  --card:      #1a2030;
  --card2:     #1f2840;
  --border:    #2a3550;
  --accent:    #c8922a;
  --accent2:   #e8b860;
  --gold:      #d4a84b;
  --text:      #e8e4d8;
  --muted:     #8a96b0;
  --success:   #4caf7d;
  --danger:    #e05c5c;
  --info:      #5b8dee;
  --verb:      #7b6ff0;
  --noun:      #4caf7d;
  --adj:       #e8854e;
  --masdar:    #5b8dee;
  --radius:    10px;
  --shadow:    0 4px 24px rgba(0,0,0,0.4);
}
*,*::before,*::after { box-sizing: border-box; margin:0; padding:0; }

body {
  background: var(--bg);
  color: var(--text);
  font-family: 'Noto Naskh Arabic', 'Amiri', serif;
  font-size: 16px;
  line-height: 1.7;
  min-height: 100vh;
}

/* Decorative background */
body::before {
  content: '';
  position: fixed;
  inset: 0;
  background:
    radial-gradient(ellipse 60% 40% at 20% 10%, rgba(200,146,42,0.08) 0%, transparent 70%),
    radial-gradient(ellipse 50% 50% at 80% 90%, rgba(91,141,238,0.06) 0%, transparent 70%);
  pointer-events: none;
  z-index: 0;
}

.container { max-width: 1300px; margin: 0 auto; padding: 0 24px; position: relative; z-index:1; }

/* ── Header ─────────────────────────────── */
header {
  border-bottom: 1px solid var(--border);
  padding: 20px 0;
  background: linear-gradient(180deg, rgba(13,15,20,0.98) 0%, transparent 100%);
  position: sticky; top:0; z-index:100;
  backdrop-filter: blur(12px);
}
.header-inner { display:flex; align-items:center; gap:20px; }
.logo {
  font-size: 2rem;
  background: linear-gradient(135deg, var(--accent), var(--accent2));
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  background-clip: text;
  font-weight: 700;
  line-height: 1;
}
.header-title { flex:1; }
.header-title h1 { font-size: 1.4rem; color: var(--text); font-weight:600; }
.header-title p  { font-size: 0.85rem; color: var(--muted); }
.stats-bar { display:flex; gap:16px; }
.stat-chip {
  background: var(--card);
  border: 1px solid var(--border);
  border-radius: 20px;
  padding: 4px 14px;
  font-size: 0.82rem;
  color: var(--muted);
  display: flex; gap:6px; align-items:center;
}
.stat-chip strong { color: var(--accent2); font-family:'IBM Plex Mono',monospace; }

/* ── Tab Navigation ─────────────────────── */
.tabs {
  display: flex;
  gap: 4px;
  padding: 20px 0 0;
  border-bottom: 1px solid var(--border);
}
.tab-btn {
  padding: 10px 22px;
  border: none; background: transparent;
  color: var(--muted);
  font-family: inherit; font-size: 0.95rem;
  cursor: pointer;
  border-radius: var(--radius) var(--radius) 0 0;
  border-bottom: 2px solid transparent;
  transition: all 0.2s;
  position: relative;
}
.tab-btn:hover { color: var(--text); background: var(--surface); }
.tab-btn.active {
  color: var(--accent2);
  border-bottom-color: var(--accent);
  background: var(--surface);
}

/* ── Panels ─────────────────────────────── */
.panel { display:none; padding: 28px 0; }
.panel.active { display:block; }

/* ── Cards ──────────────────────────────── */
.card {
  background: var(--card);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  padding: 24px;
  margin-bottom: 20px;
  box-shadow: var(--shadow);
}
.card-title {
  font-size: 1rem;
  font-weight: 600;
  color: var(--accent2);
  margin-bottom: 16px;
  padding-bottom: 10px;
  border-bottom: 1px solid var(--border);
  display: flex; align-items:center; gap:8px;
}
.card-title .icon { font-size: 1.2rem; }

/* ── Form Controls ──────────────────────── */
.form-row { display:flex; gap:12px; flex-wrap:wrap; margin-bottom:12px; align-items:flex-end; }
.form-group { flex:1; min-width:160px; }
label { display:block; font-size:0.83rem; color:var(--muted); margin-bottom:6px; }
input, select, textarea {
  width:100%;
  background: var(--bg);
  border: 1px solid var(--border);
  border-radius: 8px;
  padding: 10px 14px;
  color: var(--text);
  font-family: 'Noto Naskh Arabic', serif;
  font-size: 1.1rem;
  text-align: right;
  direction: rtl;
  transition: border-color 0.2s;
  outline: none;
}
input:focus, select:focus, textarea:focus {
  border-color: var(--accent);
  box-shadow: 0 0 0 3px rgba(200,146,42,0.15);
}
select { cursor:pointer; }

/* ── Buttons ─────────────────────────────── */
.btn {
  display: inline-flex; align-items:center; gap:8px;
  padding: 10px 22px;
  border: none; border-radius: 8px;
  font-family: inherit; font-size: 0.95rem;
  cursor: pointer; font-weight: 500;
  transition: all 0.2s;
  white-space: nowrap;
}
.btn-primary {
  background: linear-gradient(135deg, var(--accent), #a87020);
  color: #fff;
  box-shadow: 0 2px 12px rgba(200,146,42,0.3);
}
.btn-primary:hover { transform:translateY(-1px); box-shadow: 0 4px 18px rgba(200,146,42,0.45); }
.btn-secondary { background: var(--card2); color: var(--text); border:1px solid var(--border); }
.btn-secondary:hover { background: var(--border); }
.btn-danger { background: rgba(224,92,92,0.15); color:var(--danger); border:1px solid rgba(224,92,92,0.3); }
.btn-danger:hover { background: rgba(224,92,92,0.25); }
.btn-success { background: rgba(76,175,125,0.15); color:var(--success); border:1px solid rgba(76,175,125,0.3); }
.btn-success:hover { background: rgba(76,175,125,0.25); }
.btn:disabled { opacity:0.5; cursor:not-allowed; transform:none !important; }

/* ── Results ─────────────────────────────── */
.result-box {
  background: var(--bg);
  border: 1px solid var(--border);
  border-radius: 8px;
  padding: 18px;
  margin-top: 16px;
  min-height: 60px;
  font-size: 1rem;
  line-height: 1.9;
}
.result-box.hidden { display:none; }

.result-word {
  font-size: 2rem;
  color: var(--accent2);
  font-family: 'Amiri', serif;
  text-align: center;
  padding: 12px 0;
  letter-spacing: 0.05em;
}

.result-badge {
  display: inline-block;
  padding: 2px 12px;
  border-radius: 20px;
  font-size: 0.82rem;
  font-weight: 500;
}
.badge-verb    { background:rgba(123,111,240,0.15); color:var(--verb);    border:1px solid rgba(123,111,240,0.3); }
.badge-noun    { background:rgba(76,175,125,0.15);  color:var(--noun);    border:1px solid rgba(76,175,125,0.3); }
.badge-adj     { background:rgba(232,133,78,0.15);  color:var(--adj);     border:1px solid rgba(232,133,78,0.3); }
.badge-masdar  { background:rgba(91,141,238,0.15);  color:var(--masdar);  border:1px solid rgba(91,141,238,0.3); }

.valid-badge   { background:rgba(76,175,125,0.15);  color:var(--success); padding:6px 16px; border-radius:20px; font-size:1rem; border:1px solid rgba(76,175,125,0.3); }
.invalid-badge { background:rgba(224,92,92,0.15);   color:var(--danger);  padding:6px 16px; border-radius:20px; font-size:1rem; border:1px solid rgba(224,92,92,0.3); }

/* ── Derivatives Grid ────────────────────── */
.derivs-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(260px, 1fr));
  gap: 12px;
  margin-top: 16px;
}
.deriv-card {
  background: var(--card2);
  border: 1px solid var(--border);
  border-radius: 8px;
  padding: 16px;
  transition: transform 0.2s, box-shadow 0.2s;
}
.deriv-card:hover { transform:translateY(-2px); box-shadow: 0 6px 20px rgba(0,0,0,0.3); }
.deriv-word {
  font-family: 'Amiri', serif;
  font-size: 1.6rem;
  color: var(--accent2);
  margin-bottom: 6px;
}
.deriv-meta { font-size:0.82rem; color:var(--muted); }
.deriv-pattern { font-family:'IBM Plex Mono',monospace; font-size:0.85rem; color:var(--info); }

/* ── Root List ───────────────────────────── */
.root-list {
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
  max-height: 300px;
  overflow-y: auto;
  padding: 12px;
  background: var(--bg);
  border: 1px solid var(--border);
  border-radius: 8px;
}
.root-chip {
  background: var(--card2);
  border: 1px solid var(--border);
  border-radius: 6px;
  padding: 4px 14px;
  font-family: 'Amiri', serif;
  font-size: 1.2rem;
  color: var(--text);
  cursor: pointer;
  transition: all 0.15s;
}
.root-chip:hover { background:var(--accent); color:#fff; border-color:var(--accent); }

/* ── Pattern Table ───────────────────────── */
.pattern-table { width:100%; border-collapse:collapse; font-size:0.9rem; }
.pattern-table th {
  background: var(--card2);
  padding: 10px 14px;
  text-align: right;
  color: var(--muted);
  font-weight:500;
  border-bottom: 1px solid var(--border);
}
.pattern-table td { padding: 10px 14px; border-bottom: 1px solid rgba(42,53,80,0.5); }
.pattern-table tr:hover td { background: rgba(255,255,255,0.02); }
.pattern-name { font-family:'Amiri',serif; font-size:1.3rem; color:var(--accent2); }

/* ── Alert / Toast ───────────────────────── */
.toast {
  position:fixed; bottom:24px; left:50%; transform:translateX(-50%);
  background:var(--card2); border:1px solid var(--border);
  border-radius:10px; padding:12px 24px;
  font-size:0.9rem; z-index:9999;
  box-shadow: var(--shadow);
  opacity:0; transition: opacity 0.3s;
  pointer-events:none;
  min-width: 200px; text-align:center;
}
.toast.show { opacity:1; }
.toast.success { border-color:var(--success); color:var(--success); }
.toast.error   { border-color:var(--danger);  color:var(--danger);  }

/* ── Spinner ─────────────────────────────── */
.spinner {
  display:inline-block; width:16px; height:16px;
  border:2px solid var(--border);
  border-top-color: var(--accent);
  border-radius:50%;
  animation: spin 0.6s linear infinite;
  vertical-align:middle; margin-left:6px;
}
@keyframes spin { to { transform:rotate(360deg); } }

/* ── Divider ─────────────────────────────── */
.divider { height:1px; background:var(--border); margin:20px 0; }

/* ── Two-col layout ─────────────────────── */
.two-col { display:grid; grid-template-columns:1fr 1fr; gap:20px; }
@media(max-width:768px) { .two-col { grid-template-columns:1fr; } }

/* ── Search found indicator ─────────────── */
.found-yes { color: var(--success); }
.found-no  { color: var(--danger); }

/* ── Category filter ─────────────────────── */
.filter-row { display:flex; gap:8px; flex-wrap:wrap; margin-bottom:16px; }
.filter-btn {
  padding:4px 14px; border-radius:20px; border:1px solid var(--border);
  background:var(--card2); color:var(--muted);
  font-family:inherit; font-size:0.83rem; cursor:pointer; transition:all 0.15s;
}
.filter-btn.active, .filter-btn:hover { border-color:var(--accent); color:var(--accent2); }

/* scrollbar */
::-webkit-scrollbar { width:6px; }
::-webkit-scrollbar-track { background: var(--surface); }
::-webkit-scrollbar-thumb { background: var(--border); border-radius:3px; }
::-webkit-scrollbar-thumb:hover { background: var(--accent); }
</style>
</head>
<body>

<header>
  <div class="container">
    <div class="header-inner">
      <div class="logo">ص</div>
      <div class="header-title">
        <h1>محرك البحث الصرفي العربي</h1>
        <p>Arabic Morphological Search Engine &amp; Derivation Generator</p>
      </div>
      <div class="stats-bar" id="statsBar">
        <div class="stat-chip">جذور: <strong id="statRoots">…</strong></div>
        <div class="stat-chip">أوزان: <strong id="statPatterns">…</strong></div>
      </div>
    </div>
    <nav class="tabs">
      <button class="tab-btn active" data-tab="tab-generate">⚡ توليد</button>
      <button class="tab-btn" data-tab="tab-validate">✓ تحقق</button>
      <button class="tab-btn" data-tab="tab-roots">🌿 الجذور</button>
      <button class="tab-btn" data-tab="tab-patterns">📐 الأوزان</button>
      <button class="tab-btn" data-tab="tab-family">🔍 العائلة الصرفية</button>
    </nav>
  </div>
</header>

<div class="container">

<!-- ══════════════════════════════════════ GENERATE ═══════ -->
<div id="tab-generate" class="panel active">
  <div class="two-col">
    <div class="card">
      <div class="card-title"><span class="icon">⚡</span> توليد كلمة مشتقة</div>
      <div class="form-group" style="margin-bottom:12px">
        <label>الجذر الثلاثي</label>
        <input type="text" id="gen-root" placeholder="مثال: كتب" maxlength="20">
      </div>
      <div class="form-group" style="margin-bottom:16px">
        <label>الوزن الصرفي</label>
        <select id="gen-pattern">
          <option value="">-- اختر الوزن --</option>
        </select>
      </div>
      <button class="btn btn-primary" onclick="generateWord()">⚡ توليد</button>
      <div class="result-box hidden" id="gen-result"></div>
    </div>

    <div class="card">
      <div class="card-title"><span class="icon">💡</span> كيف يعمل الوزن؟</div>
      <p style="color:var(--muted);font-size:0.92rem;margin-bottom:14px">
        يستخدم النظام حروف <span style="color:var(--accent2);font-family:'Amiri',serif;font-size:1.2rem">ف ع ل</span> كرموز تعويض في قالب الوزن:
      </p>
      <div style="background:var(--bg);border:1px solid var(--border);border-radius:8px;padding:14px;font-family:'Amiri',serif;font-size:1.1rem;line-height:2.2">
        <div>الوزن: <span style="color:var(--info)">فاعِل</span></div>
        <div>الجذر: <span style="color:var(--accent2)">ك&nbsp;–&nbsp;ت&nbsp;–&nbsp;ب</span></div>
        <div style="color:var(--muted)">ف → ك، ع → ت، ل → ب</div>
        <div>النتيجة: <span style="color:var(--success);font-size:1.4rem">كاتِب</span></div>
      </div>
      <div class="divider"></div>
      <div style="background:var(--bg);border:1px solid var(--border);border-radius:8px;padding:14px;font-family:'Amiri',serif;font-size:1.1rem;line-height:2.2">
        <div>الوزن: <span style="color:var(--info)">مَفْعُول</span></div>
        <div>الجذر: <span style="color:var(--accent2)">ك&nbsp;–&nbsp;ت&nbsp;–&nbsp;ب</span></div>
        <div>النتيجة: <span style="color:var(--success);font-size:1.4rem">مَكْتُوب</span></div>
      </div>
    </div>
  </div>
</div>

<!-- ══════════════════════════════════════ VALIDATE ═══════ -->
<div id="tab-validate" class="panel">
  <div class="two-col">
    <div class="card">
      <div class="card-title"><span class="icon">✓</span> التحقق الصرفي</div>
      <p style="color:var(--muted);font-size:0.88rem;margin-bottom:16px">
        تحقق مما إذا كانت الكلمة مشتقة من جذر معين، وحدد الوزن المستخدم.
      </p>
      <div class="form-group" style="margin-bottom:12px">
        <label>الكلمة المراد فحصها</label>
        <input type="text" id="val-word" placeholder="مثال: مكتوب">
      </div>
      <div class="form-group" style="margin-bottom:16px">
        <label>الجذر المقترح</label>
        <input type="text" id="val-root" placeholder="مثال: كتب">
      </div>
      <button class="btn btn-primary" onclick="validateWord()">✓ تحقق</button>
      <div class="result-box hidden" id="val-result"></div>
    </div>

    <div class="card">
      <div class="card-title"><span class="icon">📖</span> أمثلة تحقق</div>
      <div style="display:flex;flex-direction:column;gap:10px">
        <div class="deriv-card" onclick="fillValidate('مكتوب','كتب')" style="cursor:pointer">
          <div class="deriv-word">مَكتُوب</div>
          <div class="deriv-meta">جذر: <span style="color:var(--accent2)">كتب</span></div>
        </div>
        <div class="deriv-card" onclick="fillValidate('كاتب','كتب')" style="cursor:pointer">
          <div class="deriv-word">كاتِب</div>
          <div class="deriv-meta">جذر: <span style="color:var(--accent2)">كتب</span></div>
        </div>
        <div class="deriv-card" onclick="fillValidate('معلوم','علم')" style="cursor:pointer">
          <div class="deriv-word">مَعلُوم</div>
          <div class="deriv-meta">جذر: <span style="color:var(--accent2)">علم</span></div>
        </div>
        <div class="deriv-card" onclick="fillValidate('عالم','علم')" style="cursor:pointer">
          <div class="deriv-word">عالِم</div>
          <div class="deriv-meta">جذر: <span style="color:var(--accent2)">علم</span></div>
        </div>
      </div>
    </div>
  </div>
</div>

<!-- ══════════════════════════════════════ ROOTS ══════════ -->
<div id="tab-roots" class="panel">
  <div class="two-col">
    <div class="card">
      <div class="card-title"><span class="icon">➕</span> إدارة الجذور</div>
      <div class="form-row">
        <div class="form-group">
          <label>جذر جديد</label>
          <input type="text" id="root-input" placeholder="مثال: حسب" maxlength="10">
        </div>
        <button class="btn btn-primary" onclick="insertRoot()" style="align-self:flex-end">إدراج</button>
      </div>
      <div class="form-row">
        <div class="form-group">
          <label>بحث عن جذر</label>
          <input type="text" id="root-search" placeholder="ابحث عن جذر...">
        </div>
        <button class="btn btn-secondary" onclick="searchRoot()" style="align-self:flex-end">بحث</button>
      </div>
      <div class="result-box hidden" id="root-result"></div>
    </div>

    <div class="card">
      <div class="card-title"><span class="icon">🌿</span> الجذور المخزنة في شجرة AVL</div>
      <div class="root-list" id="rootList">
        <span style="color:var(--muted);font-size:0.9rem">جارٍ التحميل...</span>
      </div>
      <p style="font-size:0.8rem;color:var(--muted);margin-top:10px">
        اضغط على جذر لاستخدامه في التوليد
      </p>
    </div>
  </div>
</div>

<!-- ══════════════════════════════════════ PATTERNS ═══════ -->
<div id="tab-patterns" class="panel">
  <div class="card">
    <div class="card-title"><span class="icon">➕</span> إضافة وزن جديد</div>
    <div class="form-row">
      <div class="form-group">
        <label>قالب الوزن (باستخدام ف، ع، ل)</label>
        <input type="text" id="pat-name" placeholder="مثال: فَعَّال">
      </div>
      <div class="form-group">
        <label>الوصف</label>
        <input type="text" id="pat-desc" placeholder="مثال: صيغة المبالغة">
      </div>
      <div class="form-group">
        <label>الفئة</label>
        <select id="pat-cat">
          <option value="اسم">اسم</option>
          <option value="فعل">فعل</option>
          <option value="صفة">صفة</option>
          <option value="مصدر">مصدر</option>
        </select>
      </div>
      <button class="btn btn-primary" onclick="addPattern()" style="align-self:flex-end">إضافة</button>
    </div>
  </div>

  <div class="card">
    <div class="card-title"><span class="icon">📐</span> جدول الأوزان الصرفية (جدول التجزئة)</div>
    <div class="filter-row" id="catFilters">
      <button class="filter-btn active" data-cat="all" onclick="filterPatterns(this,'all')">الكل</button>
      <button class="filter-btn" data-cat="فعل"  onclick="filterPatterns(this,'فعل')">أفعال</button>
      <button class="filter-btn" data-cat="اسم"  onclick="filterPatterns(this,'اسم')">أسماء</button>
      <button class="filter-btn" data-cat="صفة"  onclick="filterPatterns(this,'صفة')">صفات</button>
      <button class="filter-btn" data-cat="مصدر" onclick="filterPatterns(this,'مصدر')">مصادر</button>
    </div>
    <div style="overflow-x:auto">
      <table class="pattern-table" id="patternTable">
        <thead>
          <tr>
            <th>الوزن</th>
            <th>الوصف</th>
            <th>الفئة</th>
            <th>مثال (كتب)</th>
            <th>إجراء</th>
          </tr>
        </thead>
        <tbody id="patternTbody">
          <tr><td colspan="5" style="color:var(--muted);text-align:center;padding:20px">جارٍ التحميل…</td></tr>
        </tbody>
      </table>
    </div>
  </div>
</div>

<!-- ══════════════════════════════════════ FAMILY ═════════ -->
<div id="tab-family" class="panel">
  <div class="card">
    <div class="card-title"><span class="icon">🔍</span> العائلة الصرفية لجذر</div>
    <div class="form-row">
      <div class="form-group">
        <label>الجذر</label>
        <input type="text" id="fam-root" placeholder="مثال: كتب / علم / عمل">
      </div>
      <button class="btn btn-primary" onclick="generateFamily()">استكشاف العائلة</button>
    </div>
    <div class="filter-row hidden" id="famFilters">
      <button class="filter-btn active" data-cat="all" onclick="filterFamily(this,'all')">الكل</button>
      <button class="filter-btn" data-cat="فعل"  onclick="filterFamily(this,'فعل')">أفعال</button>
      <button class="filter-btn" data-cat="اسم"  onclick="filterFamily(this,'اسم')">أسماء</button>
      <button class="filter-btn" data-cat="صفة"  onclick="filterFamily(this,'صفة')">صفات</button>
      <button class="filter-btn" data-cat="مصدر" onclick="filterFamily(this,'مصدر')">مصادر</button>
    </div>
    <div id="fam-result" class="hidden">
      <div id="fam-header" style="margin-bottom:16px;font-size:0.9rem;color:var(--muted)"></div>
      <div class="derivs-grid" id="fam-grid"></div>
    </div>
  </div>
</div>

</div><!-- /container -->

<!-- Toast -->
<div class="toast" id="toast"></div>

<script>
const API = '';  // same origin
let allPatterns = [];
let allFamilyData = [];

// ── Tabs ──────────────────────────────────────────────────────────────────────
document.querySelectorAll('.tab-btn').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
    document.querySelectorAll('.panel').forEach(p => p.classList.remove('active'));
    btn.classList.add('active');
    document.getElementById(btn.dataset.tab).classList.add('active');
  });
});

// ── Toast ─────────────────────────────────────────────────────────────────────
function showToast(msg, type='') {
  const t = document.getElementById('toast');
  t.textContent = msg;
  t.className = 'toast show ' + type;
  setTimeout(() => t.className = 'toast', 2800);
}

// ── API helper ────────────────────────────────────────────────────────────────
async function api(endpoint, data=null) {
  const opts = data
    ? { method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify(data) }
    : { method:'GET' };
  const res = await fetch(API + endpoint, opts);
  return res.json();
}

// ── Load stats ────────────────────────────────────────────────────────────────
async function loadStats() {
  const d = await api('/api/stats');
  document.getElementById('statRoots').textContent = d.rootCount;
  document.getElementById('statPatterns').textContent = d.patternCount;
}

// ── Load patterns dropdown + table ───────────────────────────────────────────
async function loadPatterns() {
  const d = await api('/api/patterns');
  allPatterns = d.patterns || [];

  // Dropdown
  const sel = document.getElementById('gen-pattern');
  const cur = sel.value;
  sel.innerHTML = '<option value="">-- اختر الوزن --</option>';
  allPatterns.forEach(p => {
    const o = document.createElement('option');
    o.value = p.name;
    o.textContent = p.name + ' — ' + p.description;
    sel.appendChild(o);
  });
  if (cur) sel.value = cur;

  renderPatternTable('all');
}

function renderPatternTable(cat) {
  const tbody = document.getElementById('patternTbody');
  const filtered = cat === 'all' ? allPatterns : allPatterns.filter(p => p.category === cat);
  if (!filtered.length) {
    tbody.innerHTML = '<tr><td colspan="5" style="color:var(--muted);text-align:center;padding:20px">لا توجد أوزان</td></tr>';
    return;
  }
  tbody.innerHTML = filtered.map(p => {
    const badgeClass = {فعل:'badge-verb',اسم:'badge-noun',صفة:'badge-adj',مصدر:'badge-masdar'}[p.category] || '';
    const example = applyPatternClient('كتب', p.name);
    return `<tr>
      <td><span class="pattern-name">${p.name}</span></td>
      <td style="color:var(--muted)">${p.description}</td>
      <td><span class="result-badge ${badgeClass}">${p.category}</span></td>
      <td style="font-family:'Amiri',serif;font-size:1.2rem;color:var(--accent2)">${example}</td>
      <td><button class="btn btn-danger" onclick="removePattern('${escHtml(p.name)}')" style="padding:4px 10px;font-size:0.8rem">حذف</button></td>
    </tr>`;
  }).join('');
}

function filterPatterns(btn, cat) {
  document.querySelectorAll('#catFilters .filter-btn').forEach(b => b.classList.remove('active'));
  btn.classList.add('active');
  renderPatternTable(cat);
}

// Client-side pattern application (mirrors C++ logic)
function applyPatternClient(root, pattern) {
  const cps = [...root];
  if (cps.length < 3) return '';
  let result = '';
  for (const ch of pattern) {
    if (ch === 'ف') result += cps[0];
    else if (ch === 'ع') result += cps[1];
    else if (ch === 'ل') result += cps[2];
    else result += ch;
  }
  return result;
}

function escHtml(s) { return s.replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c])); }

// ── Load roots ────────────────────────────────────────────────────────────────
async function loadRoots() {
  const d = await api('/api/roots');
  const roots = d.roots || [];
  const list = document.getElementById('rootList');
  if (!roots.length) { list.innerHTML = '<span style="color:var(--muted)">لا توجد جذور</span>'; return; }
  list.innerHTML = roots.map(r =>
    `<span class="root-chip" onclick="useRoot('${escHtml(r)}')">${r}</span>`
  ).join('');
}

function useRoot(root) {
  document.getElementById('gen-root').value = root;
  document.getElementById('fam-root').value = root;
  document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
  document.querySelectorAll('.panel').forEach(p => p.classList.remove('active'));
  document.querySelector('[data-tab="tab-generate"]').classList.add('active');
  document.getElementById('tab-generate').classList.add('active');
  showToast('تم اختيار الجذر: ' + root);
}

// ── Generate ──────────────────────────────────────────────────────────────────
async function generateWord() {
  const root    = document.getElementById('gen-root').value.trim();
  const pattern = document.getElementById('gen-pattern').value;
  const box     = document.getElementById('gen-result');
  if (!root || !pattern) { showToast('الرجاء إدخال الجذر والوزن', 'error'); return; }
  box.classList.remove('hidden');
  box.innerHTML = '<span class="spinner"></span> جارٍ التوليد...';
  const d = await api('/api/generate', {root, pattern});
  if (d.success) {
    box.innerHTML = `
      <div class="result-word">${d.derivedWord}</div>
      <table style="width:100%;font-size:0.88rem;margin-top:8px">
        <tr><td style="color:var(--muted)">الجذر</td><td style="font-family:'Amiri',serif;font-size:1.1rem;color:var(--accent2)">${d.root}</td></tr>
        <tr><td style="color:var(--muted)">الوزن</td><td style="font-family:'Amiri',serif">${d.pattern}</td></tr>
        <tr><td style="color:var(--muted)">الوصف</td><td>${d.description}</td></tr>
        <tr><td style="color:var(--muted)">الفئة</td><td><span class="result-badge ${badgeClass(d.category)}">${d.category}</span></td></tr>
      </table>`;
    showToast('تم التوليد بنجاح ✓', 'success');
  } else {
    box.innerHTML = `<span style="color:var(--danger)">❌ ${d.error || 'فشل التوليد'}</span>`;
    showToast('فشل التوليد', 'error');
  }
}

// ── Validate ──────────────────────────────────────────────────────────────────
async function validateWord() {
  const word = document.getElementById('val-word').value.trim();
  const root = document.getElementById('val-root').value.trim();
  const box  = document.getElementById('val-result');
  if (!word || !root) { showToast('الرجاء إدخال الكلمة والجذر', 'error'); return; }
  box.classList.remove('hidden');
  box.innerHTML = '<span class="spinner"></span> جارٍ التحقق...';
  const d = await api('/api/validate', {word, root});
  if (d.isValid) {
    box.innerHTML = `
      <div style="text-align:center;margin-bottom:12px">
        <span class="valid-badge">✓ نعم – الكلمة مشتقة</span>
      </div>
      <table style="width:100%;font-size:0.9rem">
        <tr><td style="color:var(--muted)">الكلمة</td><td style="font-family:'Amiri',serif;font-size:1.2rem">${d.word}</td></tr>
        <tr><td style="color:var(--muted)">الجذر</td><td style="font-family:'Amiri',serif;font-size:1.2rem;color:var(--accent2)">${d.root}</td></tr>
        <tr><td style="color:var(--muted)">الوزن</td><td style="font-family:'Amiri',serif;font-size:1.1rem">${d.matchedPattern}</td></tr>
        <tr><td style="color:var(--muted)">الوصف</td><td>${d.patternDescription}</td></tr>
      </table>`;
    showToast('الكلمة مشتقة من الجذر ✓', 'success');
  } else {
    box.innerHTML = `
      <div style="text-align:center;margin-bottom:12px">
        <span class="invalid-badge">✗ لا – الكلمة ليست مشتقة</span>
      </div>
      <p style="color:var(--muted);text-align:center;margin-top:8px">${d.message}</p>`;
    showToast('الكلمة غير مشتقة من هذا الجذر', 'error');
  }
}

function fillValidate(word, root) {
  document.getElementById('val-word').value = word;
  document.getElementById('val-root').value = root;
}

// ── Root management ───────────────────────────────────────────────────────────
async function insertRoot() {
  const root = document.getElementById('root-input').value.trim();
  if (!root) { showToast('الرجاء إدخال الجذر', 'error'); return; }
  const d = await api('/api/insert-root', {root});
  const box = document.getElementById('root-result');
  box.classList.remove('hidden');
  box.innerHTML = d.inserted
    ? `<span class="found-yes">✓ تم إدراج الجذر "${root}" في الشجرة</span>`
    : `<span style="color:var(--muted)">الجذر "${root}" موجود مسبقاً</span>`;
  showToast(d.message, d.inserted ? 'success' : '');
  loadRoots(); loadStats();
}

async function searchRoot() {
  const root = document.getElementById('root-search').value.trim();
  if (!root) { showToast('الرجاء إدخال الجذر', 'error'); return; }
  const d = await api('/api/search-root', {root});
  const box = document.getElementById('root-result');
  box.classList.remove('hidden');
  box.innerHTML = d.found
    ? `<span class="found-yes">✓ الجذر "${root}" موجود في شجرة AVL</span>`
    : `<span class="found-no">✗ الجذر "${root}" غير موجود في الشجرة</span>`;
}

// ── Pattern management ────────────────────────────────────────────────────────
async function addPattern() {
  const name = document.getElementById('pat-name').value.trim();
  const desc = document.getElementById('pat-desc').value.trim();
  const cat  = document.getElementById('pat-cat').value;
  if (!name) { showToast('الرجاء إدخال قالب الوزن', 'error'); return; }
  const d = await api('/api/add-pattern', {name, desc, cat});
  showToast(d.message, d.success ? 'success' : 'error');
  if (d.success) { loadPatterns(); loadStats(); }
}

async function removePattern(name) {
  if (!confirm('هل تريد حذف الوزن: ' + name + ' ؟')) return;
  const d = await api('/api/remove-pattern', {name});
  showToast(d.message, d.success ? 'success' : 'error');
  if (d.success) { loadPatterns(); loadStats(); }
}

// ── Family exploration ────────────────────────────────────────────────────────
async function generateFamily() {
  const root = document.getElementById('fam-root').value.trim();
  if (!root) { showToast('الرجاء إدخال الجذر', 'error'); return; }
  const box = document.getElementById('fam-result');
  box.classList.remove('hidden');
  document.getElementById('fam-grid').innerHTML = '<span class="spinner"></span> جارٍ توليد العائلة الصرفية...';
  document.getElementById('fam-header').textContent = '';
  document.getElementById('famFilters').classList.remove('hidden');

  const d = await api('/api/generate-all', {root});
  allFamilyData = d.derivatives || [];
  document.getElementById('fam-header').innerHTML =
    `العائلة الصرفية للجذر <span style="color:var(--accent2);font-family:'Amiri',serif;font-size:1.2rem">${root}</span>: ${allFamilyData.length} مشتق`;
  renderFamilyGrid('all');
  showToast(`تم توليد ${allFamilyData.length} مشتق للجذر ${root}`, 'success');
}

function renderFamilyGrid(cat) {
  const filtered = cat === 'all' ? allFamilyData : allFamilyData.filter(d => d.category === cat);
  const grid = document.getElementById('fam-grid');
  if (!filtered.length) { grid.innerHTML = '<span style="color:var(--muted)">لا توجد مشتقات في هذه الفئة</span>'; return; }
  grid.innerHTML = filtered.map(d => `
    <div class="deriv-card">
      <div class="deriv-word">${d.derivedWord}</div>
      <div class="deriv-meta">${d.description}</div>
      <div style="margin-top:6px;display:flex;gap:6px;align-items:center">
        <span class="result-badge ${badgeClass(d.category)}">${d.category}</span>
        <span class="deriv-pattern">${d.pattern}</span>
      </div>
    </div>`).join('');
}

function filterFamily(btn, cat) {
  document.querySelectorAll('#famFilters .filter-btn').forEach(b => b.classList.remove('active'));
  btn.classList.add('active');
  renderFamilyGrid(cat);
}

function badgeClass(cat) {
  return {فعل:'badge-verb',اسم:'badge-noun',صفة:'badge-adj',مصدر:'badge-masdar'}[cat] || '';
}

// ── Init ──────────────────────────────────────────────────────────────────────
async function init() {
  await Promise.all([loadStats(), loadPatterns(), loadRoots()]);
}
init();
</script>
</body>
</html>)HTML";
}
