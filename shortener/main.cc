#include <drogon/drogon.h>
using namespace drogon;

int main(int argc, char* argv[]) {
    std::string configFile = "config.json";
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-c" && i + 1 < argc) {
            configFile = argv[i + 1];
        }
    }

    drogon::app().loadConfigFile(configFile);
    
    //Set HTTP listener address and port
    drogon::app().addListener("0.0.0.0", 5555);

    drogon::app()
        .registerPreRoutingAdvice([](const drogon::HttpRequestPtr &req,
                                     drogon::FilterCallback &&stop,
                                     drogon::FilterChainCallback &&pass) {
            if (req->method() == drogon::Options) {
                auto resp = drogon::HttpResponse::newHttpResponse();
                auto origin = req->getHeader("Origin");
                if (origin == "http://localhost:5173" || origin == "http://127.0.0.1:5173") {
                    resp->addHeader("Access-Control-Allow-Origin", origin);
                }
                resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
                resp->addHeader("Access-Control-Allow-Headers", "Content-Type, X-Requested-With, Authorization");
                resp->addHeader("Access-Control-Allow-Credentials", "true");
                stop(resp);
                return;
            }
            pass();
        })
        .registerPostHandlingAdvice([](const drogon::HttpRequestPtr &req, const drogon::HttpResponsePtr &resp) {
            auto origin = req->getHeader("Origin");
            if (origin == "http://localhost:5173" || origin == "http://127.0.0.1:5173") {
                resp->addHeader("Access-Control-Allow-Origin", origin);
            }
            resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            resp->addHeader("Access-Control-Allow-Headers", "Content-Type, X-Requested-With, Authorization");
            resp->addHeader("Access-Control-Allow-Credentials", "true");
        })
        .run();
    
    return 0;
}
