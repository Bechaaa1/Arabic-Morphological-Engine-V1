#pragma once
#include "MorphologyEngine.h"
#include <string>
#include <functional>
#include <map>

/**
 * Minimal single-threaded HTTP/1.1 server.
 *
 * Serves the web UI (static HTML) and exposes a JSON REST API
 * backed by the MorphologyEngine.
 *
 * Routes:
 *   GET  /                      → serve embedded HTML UI
 *   GET  /api/roots             → list all roots (JSON)
 *   GET  /api/patterns          → list all patterns (JSON)
 *   POST /api/insert-root       → body: {"root":"..."} → insert root
 *   POST /api/remove-root       → body: {"root":"..."} → remove root
 *   POST /api/search-root       → body: {"root":"..."} → search root
 *   POST /api/generate          → body: {"root":"...","pattern":"..."}
 *   POST /api/generate-all      → body: {"root":"..."} → all derivs
 *   POST /api/validate          → body: {"word":"...","root":"..."}
 *   POST /api/derivatives       → body: {"root":"..."} → stored derivs
 *   POST /api/add-pattern       → body: {"name":"...","desc":"...","cat":"..."}
 *   POST /api/remove-pattern    → body: {"name":"..."}
 */
class HttpServer {
public:
    explicit HttpServer(MorphologyEngine& engine, int port = 8080);

    /** Start listening. Blocks indefinitely. */
    void start();

private:
    MorphologyEngine& engine_;
    int port_;

    struct Request {
        std::string method;
        std::string path;
        std::string body;
        std::map<std::string, std::string> headers;
    };

    struct Response {
        int status = 200;
        std::string contentType = "application/json";
        std::string body;
    };

    // ── Request parsing ──────────────────
    Request  parseRequest(const std::string& raw);
    Response route(const Request& req);

    // ── API handlers ─────────────────────
    Response handleGetRoots();
    Response handleGetPatterns();
    Response handleInsertRoot(const std::string& body);
    Response handleRemoveRoot(const std::string& body);
    Response handleSearchRoot(const std::string& body);
    Response handleGenerate(const std::string& body);
    Response handleGenerateAll(const std::string& body);
    Response handleValidate(const std::string& body);
    Response handleDerivatives(const std::string& body);
    Response handleAddPattern(const std::string& body);
    Response handleRemovePattern(const std::string& body);
    Response handleGetStats();

    // ── Helpers ───────────────────────────
    std::string getHtmlUI() const;
    std::string buildResponse(const Response& res) const;

    // Tiny JSON helpers (no external lib)
    static std::string jsonStr(const std::string& s);
    static std::string jsonField(const std::string& k, const std::string& v);
    static std::string jsonField(const std::string& k, int v);
    static std::string jsonField(const std::string& k, bool v);
    static std::string jsonObject(const std::string& fields);
    static std::string jsonArray(const std::vector<std::string>& items);
    static std::string getJsonValue(const std::string& json, const std::string& key);
};
