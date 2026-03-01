#include "UrlController.h"
#include "models/UrlModel.h"
#include <iostream>
#include <optional>
#include <drogon/drogon.h>

using namespace drogon;
using namespace std;

// POST /shorten
void UrlController::shortenUrl(const HttpRequestPtr& req, function<void (const HttpResponsePtr &)> &&callback)
{
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

    // Validate URL
    if (!UrlModel::isValidUrl(longUrl)) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setBody("Invalid URL: must be http:// or https://, max 2048 chars");
        callback(resp);
        return;
    }

    string shortCode = UrlModel::generateShortCode(longUrl);

    if (!UrlModel::saveUrlMapping(shortCode, longUrl))
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k500InternalServerError);
        resp->setBody("Failed to save URL mapping");
        callback(resp);
        return;
    }

    // Get host from request header
    string host = req->getHeader("Host");
    if (host.empty()) {
        host = "short.url"; // fallback
    }

    Json::Value jsonResp;
    jsonResp["shortUrl"] = "http://" + host + "/" + shortCode;
    jsonResp["shortCode"] = shortCode;

    auto resp = HttpResponse::newHttpJsonResponse(jsonResp);
    callback(resp);
}

// GET /{short_code}
void UrlController::redirectToOriginal(const HttpRequestPtr& req, function<void (const HttpResponsePtr &)> &&callback, const string& short_code)
{
    auto originalUrl = UrlModel::getOriginalUrl(short_code);
    
    if(!originalUrl) {
        auto resp = HttpResponse::newNotFoundResponse();
        resp->setBody("Short URL not found");
        callback(resp);
        return;
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

    auto resp = HttpResponse::newRedirectionResponse(*originalUrl);
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