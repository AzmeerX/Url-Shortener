#include "UrlController.h"
#include "models/AuthModel.h"
#include "models/UrlModel.h"
#include <iostream>
#include <cstdint>
#include <optional>
#include <regex>
#include <unordered_set>
#include <drogon/drogon.h>

using namespace drogon;
using namespace std;

namespace {
    optional<string> extractHostFromUrl(const string &url) {
        static const regex hostRegex(R"(^https?://([^/]+))", regex::icase);
        smatch match;
        if (regex_search(url, match, hostRegex) && match.size() > 1) {
            return match[1].str();
        }
        return nullopt;
    }

    optional<string> extractPathFromUrl(const string &url) {
        static const regex pathRegex(R"(^https?://[^/]+/([^?#]+))", regex::icase);
        smatch match;
        if (regex_search(url, match, pathRegex) && match.size() > 1) {
            return match[1].str();
        }
        return nullopt;
    }

    bool isReservedPath(const string &path) {
        return path == "shorten" || path == "metrics" || path.rfind("analytics/", 0) == 0;
    }
}

// POST /shorten
void UrlController::shortenUrl(const HttpRequestPtr& req, function<void (const HttpResponsePtr &)> &&callback)
{
    auto authHeader = req->getHeader("Authorization");
    if (authHeader.empty() || authHeader.find("Bearer ") != 0) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k401Unauthorized);
        resp->setBody("Unauthorized");
        callback(resp);
        return;
    }

    string authError;
    auto userId = AuthModel::verifyToken(authHeader.substr(7), &authError);
    if (!userId) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k401Unauthorized);
        resp->setBody(authError.empty() ? "Unauthorized" : authError);
        callback(resp);
        return;
    }

    auto clientIp = req->peerAddr().toIp();
    auto redis = app().getRedisClient();

    string rateKey = "rate:" + clientIp;

    int requestCount = redis->execCommandSync<int>(
        [](const nosql::RedisResult &r) -> int {
            if (r.type() == nosql::RedisResultType::kInteger)
                return r.asInteger();
            return 0;
        },
        "INCR ?",
        rateKey
    );

    // If first request, set expiration
    if (requestCount == 1) {
        redis->execCommandSync<int>(
            [](const nosql::RedisResult &) { return 0; },
            "EXPIRE ? 60",
            rateKey
        );
    }

    // Block if exceeded
    if (requestCount > 60) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k429TooManyRequests);
        resp->setBody("Rate limit exceeded. Try again later.");
        callback(resp);
        return;
    }

    auto json = req->getJsonObject();
    if (!json || !json->isMember("long_url"))
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setBody("Invalid request: missing 'long_url' field");
        callback(resp);
        return;
    }

    string longUrl = (*json)["long_url"].asString();
    optional<int64_t> ttlSeconds = nullopt;

    if (json->isMember("ttl_seconds")) {
        if (!(*json)["ttl_seconds"].isInt64()) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(k400BadRequest);
            resp->setBody("Invalid ttl_seconds: must be a positive integer");
            callback(resp);
            return;
        }

        const auto ttl = (*json)["ttl_seconds"].asInt64();
        if (ttl <= 0 || ttl > 315360000) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(k400BadRequest);
            resp->setBody("Invalid ttl_seconds: allowed range is 1..315360000");
            callback(resp);
            return;
        }
        ttlSeconds = ttl;
    }

    // Validate URL
    if (!UrlModel::isValidUrl(longUrl)) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setBody("Invalid URL: must be http:// or https://, max 2048 chars");
        callback(resp);
        return;
    }

    string host = req->getHeader("Host");
    if (auto targetHost = extractHostFromUrl(longUrl); targetHost && !host.empty() && *targetHost == host) {
        auto path = extractPathFromUrl(longUrl);
        if (!path || path->empty() || isReservedPath(*path)) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(k400BadRequest);
            resp->setBody("Cannot shorten this shortener endpoint URL");
            callback(resp);
            return;
        }

        string currentCode = *path;
        unordered_set<string> visitedCodes;
        const int maxHops = 10;
        bool resolved = false;

        for (int hop = 0; hop < maxHops; ++hop) {
            if (!visitedCodes.insert(currentCode).second) {
                auto resp = HttpResponse::newHttpResponse();
                resp->setStatusCode(k400BadRequest);
                resp->setBody("Redirect loop detected in referenced short URL");
                callback(resp);
                return;
            }

            auto mapped = UrlModel::getOriginalUrl(currentCode);
            if (!mapped) {
                auto resp = HttpResponse::newNotFoundResponse();
                resp->setBody("Referenced short URL not found");
                callback(resp);
                return;
            }

            if (auto nestedHost = extractHostFromUrl(*mapped); nestedHost && *nestedHost == host) {
                auto nestedPath = extractPathFromUrl(*mapped);
                if (!nestedPath || nestedPath->empty() || isReservedPath(*nestedPath)) {
                    auto resp = HttpResponse::newHttpResponse();
                    resp->setStatusCode(k400BadRequest);
                    resp->setBody("Referenced short URL resolves to an invalid internal path");
                    callback(resp);
                    return;
                }
                currentCode = *nestedPath;
                continue;
            }

            longUrl = *mapped;
            resolved = true;
            break;
        }

        if (!resolved) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(k400BadRequest);
            resp->setBody("Too many chained short URL redirects");
            callback(resp);
            return;
        }
    }

    string shortCode = UrlModel::generateShortCode(longUrl);
    auto savedShortCode = UrlModel::saveUrlMapping(shortCode, longUrl, ttlSeconds, userId);
    if (!savedShortCode)
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k500InternalServerError);
        resp->setBody("Failed to save URL mapping");
        callback(resp);
        return;
    }

    // Get host from request header
    if (host.empty()) {
        host = "short.url"; // fallback
    }

    Json::Value jsonResp;
    jsonResp["shortUrl"] = "http://" + host + "/" + *savedShortCode;
    jsonResp["shortCode"] = *savedShortCode;
    if (ttlSeconds) {
        jsonResp["ttlSeconds"] = static_cast<Json::Int64>(*ttlSeconds);
    }

    auto resp = HttpResponse::newHttpJsonResponse(jsonResp);
    callback(resp);
}

// GET /{short_code}
void UrlController::redirectToOriginal(const HttpRequestPtr& req, function<void (const HttpResponsePtr &)> &&callback, const string& short_code)
{
    auto originalUrl = UrlModel::getOriginalUrl(short_code);
    
    if(!originalUrl) {
        LOG_WARN << "No URL found for short code: " << short_code;
        auto resp = HttpResponse::newNotFoundResponse();
        resp->setBody("Short URL not found");
        callback(resp);
        return;
    }

    LOG_INFO << "Redirecting " << short_code << " to: " << *originalUrl;

    string host = req->getHeader("Host");
    if (!host.empty()) {
        const string sameHttpUrl = "http://" + host + "/" + short_code;
        const string sameHttpsUrl = "https://" + host + "/" + short_code;
        if (*originalUrl == sameHttpUrl || *originalUrl == sameHttpsUrl) {
            LOG_ERROR << "Detected self-referential redirect loop for short code: " << short_code;
            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(k500InternalServerError);
            resp->setBody("Redirect loop detected for this short URL");
            callback(resp);
            return;
        }
    }

    // Check expiration by querying the database directly in the controller
    try {
        auto client = drogon::app().getDbClient("default");
        auto result = client->execSqlSync(
            "SELECT expires_at FROM url_mapping WHERE short_code=$1",
            short_code
        );

        if (!result.empty() && !result[0]["expires_at"].isNull()) {
            string expiresAt = result[0]["expires_at"].as<string>();
            // If expires_at exists and is in the past, return 410 Gone
            // For simplicity, we check if expires_at < NOW()
            auto checkExpired = client->execSqlSync(
                "SELECT CASE WHEN expires_at < NOW() THEN 1 ELSE 0 END as is_expired FROM url_mapping WHERE short_code=$1",
                short_code
            );

            if (!checkExpired.empty() && checkExpired[0]["is_expired"].as<int>() == 1) {
                auto resp = HttpResponse::newHttpResponse();
                resp->setStatusCode(k410Gone);
                resp->setBody("Short URL has expired");
                callback(resp);
                return;
            }
        }
    } catch (const exception& e) {
        LOG_ERROR << "Expiration check failed: " << e.what();
        // Continue anyway if expiration check fails
    }

    UrlModel::incrementClickCount(short_code);

    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(drogon::k302Found);
    resp->addHeader("Location", *originalUrl);
    callback(resp);
}

// GET /analytics/{short_code}
void UrlController::getAnalytics(const HttpRequestPtr& req, function<void (const HttpResponsePtr &)> &&callback, const string& short_code)
{
    auto analytics = UrlModel::getAnalytics(short_code);
    
    if (!analytics) {
        auto resp = HttpResponse::newNotFoundResponse();
        resp->setBody("Short URL not found");
        callback(resp);
        return;
    }

    auto resp = HttpResponse::newHttpJsonResponse(*analytics);
    callback(resp);
}
