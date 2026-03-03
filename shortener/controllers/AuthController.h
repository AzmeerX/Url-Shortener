#pragma once

#include <drogon/HttpController.h>

using namespace drogon;
using namespace std;

class AuthController : public HttpController<AuthController>
{
  public:
    METHOD_LIST_BEGIN

    ADD_METHOD_TO(AuthController::registerUser, "/register", Post);
    ADD_METHOD_TO(AuthController::login, "/login", Post);

    METHOD_LIST_END

    void login(const HttpRequestPtr& req, function<void (const HttpResponsePtr &)> &&callback);
    void registerUser(const HttpRequestPtr& req, function<void (const HttpResponsePtr &)> &&callback);
};
