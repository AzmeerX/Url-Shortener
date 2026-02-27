#pragma once

#include <drogon/HttpSimpleController.h>

using namespace drogon;
using namespace std;

class UrlController : public drogon::HttpSimpleController<UrlController>
{
  public:
    METHOD_LIST_BEGIN

    ADD_METHOD_TO(UrlController::shortenUrl, "/shorten", Post);

    ADD_METHOD_TO(UrlController::redirectToOriginal, "/{short_code}", Get);

    METHOD_LIST_END

    void shortenUrl(const HttpRequestPtr& req, function<void (const HttpRequestPtr &)> &&callback);
    void redirectToOriginal(const HttpRequestPtr& req, function<void (const HttpRequestPtr &)> &&callback, string shortCode);
};
