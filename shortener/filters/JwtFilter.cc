#include "JwtFilter.h"
#include "models/AuthModel.h"

using namespace drogon;
using namespace std;

void JwtFilter::doFilter(const HttpRequestPtr &req,
                         FilterCallback &&fcb,
                         FilterChainCallback &&fccb)
{
    auto authHeader = req->getHeader("Authorization");

    if (authHeader.empty() || authHeader.find("Bearer ") != 0)
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k401Unauthorized);
        resp->setBody("Unauthorized");
        fcb(resp);
        return;
    }

    string token = authHeader.substr(7);

    string authError;
    auto userId = AuthModel::verifyToken(token, &authError);
    if (!userId) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k401Unauthorized);
        resp->setBody(authError.empty() ? "Unauthorized" : authError);
        fcb(resp);
        return;
    }

    req->attributes()->insert("user_id", *userId);

    fccb();
}
