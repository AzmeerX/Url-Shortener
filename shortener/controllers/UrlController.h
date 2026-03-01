#pragma once

#include <drogon/HttpController.h>

using namespace drogon;
using namespace std;

class UrlController : public HttpController<UrlController>
{
  public:
    METHOD_LIST_BEGIN

    ADD_METHOD_TO(UrlController::shortenUrl, "/shorten", Post);

    ADD_METHOD_TO(UrlController::redirectToOriginal, "/{short_code}", Get);

    ADD_METHOD_TO(UrlController::getAnalytics, "/analytics/{short_code}", Get);

    METHOD_LIST_END

    void shortenUrl(const HttpRequestPtr& req, function<void (const HttpResponsePtr &)> &&callback);

    void redirectToOriginal(const HttpRequestPtr& req, function<void (const HttpResponsePtr &)> &&callback, const string& short_code);

    void getAnalytics(const HttpRequestPtr& req, function<void (const HttpResponsePtr &)> &&callback, const string& short_code);
};
