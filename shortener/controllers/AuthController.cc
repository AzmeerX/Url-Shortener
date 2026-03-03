#include "AuthController.h"
#include "models/AuthModel.h"

using namespace drogon;
using namespace std;

namespace {
optional<string> extractEmailOrUsername(const Json::Value &json) {
    if (json.isMember("email") && json["email"].isString()) {
        return json["email"].asString();
    }
    if (json.isMember("username") && json["username"].isString()) {
        return json["username"].asString();
    }
    return nullopt;
}
}

void AuthController::login(const HttpRequestPtr& req, function<void (const HttpResponsePtr &)> &&callback)
{
    auto json = req->getJsonObject();
    auto emailOrUsername = json ? extractEmailOrUsername(*json) : nullopt;
    if (!json || !emailOrUsername || !(*json)["password"].isString())
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setBody("Invalid payload: email (or username) and password are required");
        callback(resp);
        return;
    }

    string email = *emailOrUsername;
    string password = (*json)["password"].asString();
    auto userId = AuthModel::authenticateUser(email, password);
    if (!userId) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k401Unauthorized);
        resp->setBody("Invalid credentials");
        callback(resp);
        return;
    }

    Json::Value data;
    data["token"] = AuthModel::generateToken(*userId);
    data["userId"] = *userId;
    data["email"] = email;

    auto resp = HttpResponse::newHttpJsonResponse(data);
    callback(resp);
}

void AuthController::registerUser(const HttpRequestPtr& req, function<void (const HttpResponsePtr &)> &&callback)
{
    auto json = req->getJsonObject();
    auto emailOrUsername = json ? extractEmailOrUsername(*json) : nullopt;
    if (!json || !emailOrUsername || !(*json)["password"].isString())
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setBody("Invalid payload: email (or username) and password are required");
        callback(resp);
        return;
    }

    string email = *emailOrUsername;
    string password = (*json)["password"].asString();

    if (!AuthModel::isValidEmail(email)) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setBody("Invalid email format");
        callback(resp);
        return;
    }

    if (!AuthModel::isStrongPassword(password)) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setBody("Password must be between 8 and 128 characters");
        callback(resp);
        return;
    }

    string error;
    auto userId = AuthModel::registerUser(email, password, &error);
    if (!userId) {
        auto resp = HttpResponse::newHttpResponse();
        if (error == "User already exists") {
            resp->setStatusCode(k409Conflict);
            resp->setBody(error);
        } else {
            resp->setStatusCode(k500InternalServerError);
            resp->setBody(error.empty() ? "Registration failed" : error);
        }
        callback(resp);
        return;
    }

    Json::Value data;
    data["message"] = "User registered successfully";
    data["token"] = AuthModel::generateToken(*userId);
    data["userId"] = *userId;
    data["email"] = email;

    auto resp = HttpResponse::newHttpJsonResponse(data);
    resp->setStatusCode(k201Created);
    callback(resp);
}
