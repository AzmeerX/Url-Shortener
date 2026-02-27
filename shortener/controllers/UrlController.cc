#include "UrlContrller.h"
#include <iostream>
#include <optional>
#include <drogon/drogon.h>

using namespace drogon;
using namespace std;

void UrlController::asyncHandleHttpRequest(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback)
{
    // write your application logic here
    string generateShortCode(const string& longUrl);
    bool saveUrlMapping(const string& shortCode, const string& longUrl);
    optional<string> getLongUrl(const string& shortCode);

    // POST /shorten
    void UrlController::shortenUrl(const HttpRequestPtr& req, function<void (const HttpResponsePtr &)> &&callback)
    {
        auto json = req->getJsonObject();
        if (!json || !json->isMember("longUrl"))
        {
            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(k400BadRequest);
            resp->setBody("Invalid request");
            callback(resp);
            return;
        }

        string longUrl = (*json)["longUrl"].asString();
        string shortCode = generateShortCode(longUrl);

        if (!saveUrlMapping(shortCode, longUrl))
        {
            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(k500InternalServerError);
            resp->setBody("Failed to save URL mapping");
            callback(resp);
            return;
        }

        auto resp = HttpResponse::newHttpJsonResponse({{"shortUrl", "http://short.url/" + shortCode}});
        callback(resp);
    }

    // GET /{short_code}
    void UrlController::redirectToOriginal(const HttpRequestPtr& req, function<void (const HttpResponsePtr &)> &&callback, string shortCode)
    {
        auto originalUrl = getOriginalUrl(shortCode);
        
        if(originalUrl) {
            auto resp = HTTPResponse::newRedirectResponse(*originalUrl);
            callback(resp);
        }
        else {
            callback(HttpResponse::newNotFoundResponse("Short URL not found"));
        }
    }
}
