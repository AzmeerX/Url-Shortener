#include "UrlModel.h"
#include <drogon/drogon.h>
#include <functional> 
#include <cstdint>
#include <chrono>
#include <regex>

using namespace drogon;
using namespace std;

// Base62 generatore
string UrlModel::generateShortCode(const string& longUrl) {
    static const char base62[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

    string code;
    size_t hashValue = std::hash<std::string>{}(longUrl);

    for (int i = 0; i < 6; ++i)
    {
        code += base62[hashValue % 62];
        hashValue /= 62;
    }

    return code;
}

// Insert mapping in DB (with collision retry)
optional<string> UrlModel::saveUrlMapping(const string& shortCode, const string& longUrl, const optional<int64_t>& ttlSeconds) {
    if (!isValidUrl(longUrl)) {
        LOG_ERROR << "Invalid URL: " << longUrl;
        return nullopt;
    }

    if (!ttlSeconds) {
        try {
            auto client = app().getDbClient("default");
            auto existing = client->execSqlSync(
                "SELECT short_code FROM url_mapping "
                "WHERE original_url=$1 AND (expires_at IS NULL OR expires_at > NOW()) "
                "ORDER BY created_at ASC LIMIT 1",
                longUrl
            );

            if (!existing.empty()) {
                const string existingCode = existing[0]["short_code"].as<string>();
                LOG_INFO << "Reusing existing short code for URL: " << existingCode;
                return existingCode;
            }
        } catch (const exception& e) {
            LOG_WARN << "Existing mapping lookup failed, continuing with insert flow: " << e.what();
        }
    }

    const int MAX_RETRIES = 8;
    string currentShortCode = shortCode;

    for (int attempt = 0; attempt < MAX_RETRIES; ++attempt) {
        try {
            auto client = app().getDbClient("default");

            LOG_INFO << "Saving (attempt " << (attempt + 1) << "): shortCode=" << currentShortCode << ", longUrl=" << longUrl;

            if (ttlSeconds) {
                const string ttlInterval = to_string(*ttlSeconds) + " seconds";

                auto result = client->execSqlSync(
                    "INSERT INTO url_mapping(short_code, original_url, expires_at) "
                    "VALUES($1, $2, NOW() + $3::interval) "
                    "RETURNING short_code",
                    currentShortCode,
                    longUrl,
                    ttlInterval
                );

                if (!result.empty()) {
                    LOG_INFO << "Successfully saved short code: " << currentShortCode;
                    return currentShortCode;
                }
            } else {
                auto result = client->execSqlSync(
                    "INSERT INTO url_mapping(short_code, original_url) "
                    "VALUES($1, $2) "
                    "RETURNING short_code",
                    currentShortCode,
                    longUrl
                );

                if (!result.empty()) {
                    LOG_INFO << "Successfully saved short code: " << currentShortCode;
                    return currentShortCode;
                }
            }

            // Collision detected, generate new code and retry
            LOG_WARN << "Short code collision detected: " << currentShortCode << ", retrying...";
            currentShortCode = generateShortCode(longUrl + to_string(attempt));
        }
        catch(const exception& e) {
            LOG_ERROR << "DB insert attempt " << (attempt + 1) << " failed: " << e.what();
            if (attempt < MAX_RETRIES - 1) {
                const auto nonce = chrono::high_resolution_clock::now().time_since_epoch().count();
                currentShortCode = generateShortCode(longUrl + ":" + to_string(attempt + 1) + ":" + to_string(nonce));
            }
        }
    }

    LOG_ERROR << "Failed to save URL after " << MAX_RETRIES << " attempts";
    return nullopt;
}

bool UrlModel::incrementClickCount(const string& shortCode) {
    try {
        auto client = app().getDbClient("default");
        client->execSqlSync(
            "UPDATE url_mapping SET click_count = click_count + 1 WHERE short_code=$1",
            shortCode
        );
        return true;
    } catch (const exception& e) {
        LOG_ERROR << "Failed to increment click count for " << shortCode << ": " << e.what();
        return false;
    }
}

// Fetch original URL with expiration check
optional<string> UrlModel::getOriginalUrl(const string& shortCode)
{
    try {   
        nosql::RedisClientPtr redis = app().getRedisClient();
        
        // 1. Check Redis cache first
        auto cached = redis->execCommandSync<string>(
            [](const nosql::RedisResult &r) -> string {
                if (r.type() == nosql::RedisResultType::kString) {
                    return r.asString();
                }
                return ""; 
            },
            "GET %s",
            shortCode.c_str()
        );

        if (!cached.empty() && isValidUrl(cached)) {
            LOG_INFO << "Cache hit for short code: " << shortCode << " -> " << cached;
            return cached;
        }
        
        LOG_INFO << "Cache miss, querying database for short code: " << shortCode;
        auto client = app().getDbClient("default");

        // 2. Query database for original URL
        auto result = client->execSqlSync(
            "SELECT original_url, expires_at "
            "FROM url_mapping WHERE short_code=$1",
            shortCode
        );

        if (result.empty()) {
            LOG_WARN << "No URL found for short code: " << shortCode;
            return nullopt;
        }

        string originalUrl = result[0]["original_url"].as<string>();
        LOG_INFO << "Found in DB - shortCode: " << shortCode << " -> " << originalUrl;

        redis->execCommandSync<int>(
            [](const nosql::RedisResult &) { return 0; },
            "SETEX %s 3600 %s",
            shortCode.c_str(),
            originalUrl.c_str()
        );

        LOG_INFO << "Cached in Redis: " << shortCode << " -> " << originalUrl;
        return originalUrl;
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "Data fetch error: " << e.what();
        return nullopt;
    }
}

// Validate URL format
bool UrlModel::isValidUrl(const string& url) {
    if (url.length() > 2048) {
        LOG_WARN << "URL exceeds max length (2048 chars): " << url.length();
        return false;
    }

    // Simple validation: must start with http:// or https://
    if (url.substr(0, 7) != "http://" && url.substr(0, 8) != "https://") {
        LOG_WARN << "URL must start with http:// or https://: " << url;
        return false;
    }

    // Basic regex validation
    regex urlRegex("^https?://[a-zA-Z0-9\\-._~:/?#\\[\\]@!$&'()*+,;=%]+$");
    if (!regex_match(url, urlRegex)) {
        LOG_WARN << "URL format invalid: " << url;
        return false;
    }

    return true;
}

// Get analytics for a short code
optional<Json::Value> UrlModel::getAnalytics(const string& shortCode) {
    try {
        auto client = app().getDbClient("default");

        LOG_INFO << "Fetching analytics for: " << shortCode;

        auto result = client->execSqlSync(
            "SELECT short_code, original_url, click_count, created_at, expires_at "
            "FROM url_mapping WHERE short_code=$1",
            shortCode
        );

        if (result.empty()) {
            LOG_WARN << "No URL found for analytics: " << shortCode;
            return nullopt;
        }

        Json::Value analytics;
        analytics["shortCode"] = result[0]["short_code"].as<string>();
        analytics["originalUrl"] = result[0]["original_url"].as<string>();
        analytics["clickCount"] = result[0]["click_count"].as<int>();
        analytics["createdAt"] = result[0]["created_at"].as<string>();

        if (!result[0]["expires_at"].isNull()) {
            analytics["expiresAt"] = result[0]["expires_at"].as<string>();
        } else {
            analytics["expiresAt"] = Json::Value();
        }

        return analytics;
    }
    catch (const exception& e) {
        LOG_ERROR << "Analytics fetch failed: " << e.what();
        return nullopt;
    }
}
