#include <drogon/drogon.h>
#include <cstdlib>
#include <fstream>
#include <json/json.h>
using namespace drogon;

static bool hasEnv(const char *key) {
    const char *value = std::getenv(key);
    return value && *value;
}

static std::string getEnv(const char *key) {
    const char *value = std::getenv(key);
    return value ? std::string(value) : std::string();
}

static void overrideString(Json::Value &node, const char *key) {
    if (hasEnv(key)) {
        node = getEnv(key);
    }
}

static void overrideInt(Json::Value &node, const char *key) {
    if (!hasEnv(key)) {
        return;
    }
    try {
        node = std::stoi(getEnv(key));
    } catch (...) {
        // Ignore invalid values and keep existing config.
    }
}

int main(int argc, char* argv[]) {
    std::string configFile = "config.json";
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-c" && i + 1 < argc) {
            configFile = argv[i + 1];
        }
    }

    Json::Value config;
    {
        std::ifstream input(configFile);
        if (!input.good()) {
            std::cerr << "Failed to open config file: " << configFile << std::endl;
            return 1;
        }
        Json::CharReaderBuilder builder;
        std::string errors;
        if (!Json::parseFromStream(builder, input, &config, &errors)) {
            std::cerr << "Failed to parse config file: " << errors << std::endl;
            return 1;
        }
    }

    // Ensure db_clients[0] exists.
    if (!config.isMember("db_clients") || !config["db_clients"].isArray() ||
        config["db_clients"].empty()) {
        config["db_clients"] = Json::arrayValue;
        config["db_clients"].append(Json::Value(Json::objectValue));
    }
    Json::Value &db = config["db_clients"][0];
    // Support both custom env vars and Railway/Render defaults
    overrideString(db["host"], "DB_HOST");
    if (!hasEnv("DB_HOST")) overrideString(db["host"], "PGHOST");
    overrideInt(db["port"], "DB_PORT");
    if (!hasEnv("DB_PORT")) overrideInt(db["port"], "PGPORT");
    overrideString(db["dbname"], "DB_NAME");
    if (!hasEnv("DB_NAME")) overrideString(db["dbname"], "POSTGRES_DB");
    overrideString(db["user"], "DB_USER");
    if (!hasEnv("DB_USER")) overrideString(db["user"], "PGUSER");
    overrideString(db["password"], "DB_PASS");
    if (!hasEnv("DB_PASS")) overrideString(db["password"], "PGPASSWORD");
    if (hasEnv("DB_SSLMODE") || hasEnv("DB_CHANNEL_BINDING")) {
        Json::Value &opts = db["connect_options"];
        if (!opts.isObject()) {
            opts = Json::objectValue;
        }
        overrideString(opts["sslmode"], "DB_SSLMODE");
        // Channel binding may not be supported by libpq - skip it for now
        // overrideString(opts["channel_binding"], "DB_CHANNEL_BINDING");
    }

    // Ensure redis_clients[0] exists.
    // DISABLED: We don't use Redis, so disable it to avoid connection errors
    /*
    if (!config.isMember("redis_clients") || !config["redis_clients"].isArray() ||
        config["redis_clients"].empty()) {
        config["redis_clients"] = Json::arrayValue;
        config["redis_clients"].append(Json::Value(Json::objectValue));
    }
    Json::Value &redis = config["redis_clients"][0];
    // Support both custom env vars and Railway/Render defaults
    overrideString(redis["host"], "REDIS_HOST");
    if (!hasEnv("REDIS_HOST")) overrideString(redis["host"], "REDISHOST");
    overrideInt(redis["port"], "REDIS_PORT");
    if (!hasEnv("REDIS_PORT")) overrideInt(redis["port"], "REDISPORT");
    overrideString(redis["username"], "REDIS_USER");
    if (!hasEnv("REDIS_USER")) overrideString(redis["username"], "REDISUSER");
    overrideString(redis["passwd"], "REDIS_PASS");
    if (!hasEnv("REDIS_PASS")) overrideString(redis["passwd"], "REDIS_PASSWORD");
    */
    // Ensure redis_clients is empty
    if (!config.isMember("redis_clients")) {
        config["redis_clients"] = Json::arrayValue;
    }

    const std::string resolvedConfig = "/tmp/shortener_config.json";
    {
        std::ofstream output(resolvedConfig);
        output << config.toStyledString();
    }

    drogon::app().loadConfigFile(resolvedConfig);
    
    // Set HTTP listener address and port (Render provides PORT).
    int listenPort = 5555;
    if (hasEnv("PORT")) {
        try {
            listenPort = std::stoi(getEnv("PORT"));
        } catch (...) {
            listenPort = 5555;
        }
    }
    drogon::app().addListener("0.0.0.0", listenPort);

    const std::string frontendOrigin = hasEnv("FRONTEND_ORIGIN") ? getEnv("FRONTEND_ORIGIN") : std::string();

    drogon::app()
        .registerPreRoutingAdvice([frontendOrigin](const drogon::HttpRequestPtr &req,
                                     drogon::FilterCallback &&stop,
                                     drogon::FilterChainCallback &&pass) {
            auto origin = req->getHeader("Origin");
            auto resp = drogon::HttpResponse::newHttpResponse();
            
            // Check if origin is allowed
            bool originAllowed = origin == "http://localhost:5173" || 
                                origin == "http://127.0.0.1:5173" ||
                                origin == frontendOrigin;
            
            if (req->method() == drogon::Options) {
                if (originAllowed) {
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
        .registerPostHandlingAdvice([frontendOrigin](const drogon::HttpRequestPtr &req, const drogon::HttpResponsePtr &resp) {
            auto origin = req->getHeader("Origin");
            bool originAllowed = origin == "http://localhost:5173" || 
                                origin == "http://127.0.0.1:5173" ||
                                origin == frontendOrigin;
            
            if (originAllowed) {
                resp->addHeader("Access-Control-Allow-Origin", origin);
            }
            resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            resp->addHeader("Access-Control-Allow-Headers", "Content-Type, X-Requested-With, Authorization");
            resp->addHeader("Access-Control-Allow-Credentials", "true");
        })
        .run();
    
    return 0;
}
