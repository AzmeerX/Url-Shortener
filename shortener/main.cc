#include <drogon/drogon.h>
#include <cctype>
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

struct RedisUrlParts {
    std::string host;
    int port = -1;
    std::string username;
    std::string password;
    int db = -1;
    bool ok = false;
};

static int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

static std::string urlDecode(const std::string &in) {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '%' && i + 2 < in.size()) {
            int hi = hexValue(in[i + 1]);
            int lo = hexValue(in[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        out.push_back(in[i]);
    }
    return out;
}

static std::string trimCopy(const std::string &in) {
    size_t start = 0;
    while (start < in.size() && std::isspace(static_cast<unsigned char>(in[start]))) {
        ++start;
    }
    size_t end = in.size();
    while (end > start && std::isspace(static_cast<unsigned char>(in[end - 1]))) {
        --end;
    }
    return in.substr(start, end - start);
}

static bool parseRedisUrl(const std::string &url, RedisUrlParts &out) {
    auto schemePos = url.find("://");
    if (schemePos == std::string::npos) {
        return false;
    }
    std::string rest = url.substr(schemePos + 3);
    auto queryPos = rest.find('?');
    if (queryPos != std::string::npos) {
        rest = rest.substr(0, queryPos);
    }

    std::string hostPart = rest;
    std::string pathPart;
    auto slashPos = rest.find('/');
    if (slashPos != std::string::npos) {
        hostPart = rest.substr(0, slashPos);
        pathPart = rest.substr(slashPos + 1);
    }

    std::string authPart;
    std::string hostPortPart = hostPart;
    auto atPos = hostPart.rfind('@');
    if (atPos != std::string::npos) {
        authPart = hostPart.substr(0, atPos);
        hostPortPart = hostPart.substr(atPos + 1);
    }

    if (!authPart.empty()) {
        auto colonPos = authPart.find(':');
        if (colonPos != std::string::npos) {
            out.username = urlDecode(authPart.substr(0, colonPos));
            out.password = urlDecode(authPart.substr(colonPos + 1));
        } else {
            out.username = urlDecode(authPart);
        }
    }

    if (hostPortPart.empty()) {
        return false;
    }

    if (hostPortPart.front() == '[') {
        auto end = hostPortPart.find(']');
        if (end == std::string::npos) {
            return false;
        }
        out.host = hostPortPart.substr(1, end - 1);
        if (end + 1 < hostPortPart.size() && hostPortPart[end + 1] == ':') {
            try {
                out.port = std::stoi(hostPortPart.substr(end + 2));
            } catch (...) {
                out.port = -1;
            }
        }
    } else {
        auto colonPos = hostPortPart.rfind(':');
        if (colonPos != std::string::npos && hostPortPart.find(':') == colonPos) {
            out.host = hostPortPart.substr(0, colonPos);
            try {
                out.port = std::stoi(hostPortPart.substr(colonPos + 1));
            } catch (...) {
                out.port = -1;
            }
        } else {
            out.host = hostPortPart;
        }
    }

    if (!pathPart.empty()) {
        bool allDigits = true;
        for (char c : pathPart) {
            if (!std::isdigit(static_cast<unsigned char>(c))) {
                allDigits = false;
                break;
            }
        }
        if (allDigits) {
            try {
                out.db = std::stoi(pathPart);
            } catch (...) {
                out.db = -1;
            }
        }
    }

    out.ok = !out.host.empty();
    return out.ok;
}

static bool parseRedisUrlAny(const std::string &url, RedisUrlParts &out) {
    std::string cleaned = trimCopy(url);
    if (cleaned.size() >= 2 &&
        ((cleaned.front() == '"' && cleaned.back() == '"') ||
         (cleaned.front() == '\'' && cleaned.back() == '\''))) {
        cleaned = cleaned.substr(1, cleaned.size() - 2);
    }

    if (parseRedisUrl(cleaned, out)) {
        return true;
    }

    // Fallback: very tolerant parsing to extract host/port.
    std::string work = cleaned;
    auto schemePos = work.find("://");
    if (schemePos != std::string::npos) {
        work = work.substr(schemePos + 3);
    }
    auto atPos = work.rfind('@');
    if (atPos != std::string::npos) {
        work = work.substr(atPos + 1);
    }
    auto slashPos = work.find('/');
    if (slashPos != std::string::npos) {
        work = work.substr(0, slashPos);
    }
    if (work.empty()) {
        return false;
    }

    if (work.front() == '[') {
        auto end = work.find(']');
        if (end == std::string::npos) {
            return false;
        }
        out.host = work.substr(1, end - 1);
        if (end + 1 < work.size() && work[end + 1] == ':') {
            try {
                out.port = std::stoi(work.substr(end + 2));
            } catch (...) {
                out.port = -1;
            }
        }
    } else {
        auto colonPos = work.rfind(':');
        if (colonPos != std::string::npos) {
            out.host = work.substr(0, colonPos);
            try {
                out.port = std::stoi(work.substr(colonPos + 1));
            } catch (...) {
                out.port = -1;
            }
        } else {
            out.host = work;
        }
    }

    out.ok = !out.host.empty();
    return out.ok;
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
    if (!config.isMember("redis_clients") || !config["redis_clients"].isArray() ||
        config["redis_clients"].empty()) {
        config["redis_clients"] = Json::arrayValue;
        config["redis_clients"].append(Json::Value(Json::objectValue));
    }
    Json::Value &redis = config["redis_clients"][0];
    // DEBUG: Print which redis env vars are visible (no secrets).
    std::cerr << "=== REDIS ENV ===" << std::endl;
    std::cerr << "REDIS_URL: " << (hasEnv("REDIS_URL") ? "set" : "not set") << std::endl;
    std::cerr << "REDIS_PUBLIC_URL: " << (hasEnv("REDIS_PUBLIC_URL") ? "set" : "not set") << std::endl;
    if (hasEnv("REDIS_HOST")) {
        std::cerr << "REDIS_HOST: " << getEnv("REDIS_HOST") << std::endl;
    } else {
        std::cerr << "REDIS_HOST: not set" << std::endl;
    }
    if (hasEnv("REDISHOST")) {
        std::cerr << "REDISHOST: " << getEnv("REDISHOST") << std::endl;
    } else {
        std::cerr << "REDISHOST: not set" << std::endl;
    }
    if (hasEnv("REDIS_PORT")) {
        std::cerr << "REDIS_PORT: " << getEnv("REDIS_PORT") << std::endl;
    } else if (hasEnv("REDISPORT")) {
        std::cerr << "REDISPORT: " << getEnv("REDISPORT") << std::endl;
    } else {
        std::cerr << "REDIS_PORT: not set" << std::endl;
    }
    std::cerr << "REDIS_USER: " << (hasEnv("REDIS_USER") ? getEnv("REDIS_USER") : "not set") << std::endl;
    if (!hasEnv("REDIS_USER")) {
        std::cerr << "REDIS_USERNAME: " << (hasEnv("REDIS_USERNAME") ? getEnv("REDIS_USERNAME") : "not set") << std::endl;
    }
    std::cerr << "REDIS_PASSWORD: " << (hasEnv("REDIS_PASSWORD") ? "set" : "not set") << std::endl;
    std::cerr << "=================" << std::endl;
    // Support both custom env vars and Railway/Render defaults
    if (hasEnv("REDIS_URL")) {
        RedisUrlParts parts;
        if (parseRedisUrlAny(getEnv("REDIS_URL"), parts)) {
            redis["host"] = parts.host;
            if (parts.port > 0) {
                redis["port"] = parts.port;
            }
            if (!parts.username.empty()) {
                redis["username"] = parts.username;
            }
            if (!parts.password.empty()) {
                redis["passwd"] = parts.password;
            }
            if (parts.db >= 0) {
                redis["db"] = parts.db;
            }
            std::cerr << "REDIS_URL parsed host: " << parts.host << " port: " << (parts.port > 0 ? parts.port : redis["port"].asInt()) << std::endl;
        } else {
            std::cerr << "REDIS_URL parse failed" << std::endl;
        }
    } else if (hasEnv("REDIS_PUBLIC_URL")) {
        RedisUrlParts parts;
        if (parseRedisUrlAny(getEnv("REDIS_PUBLIC_URL"), parts)) {
            redis["host"] = parts.host;
            if (parts.port > 0) {
                redis["port"] = parts.port;
            }
            if (!parts.username.empty()) {
                redis["username"] = parts.username;
            }
            if (!parts.password.empty()) {
                redis["passwd"] = parts.password;
            }
            if (parts.db >= 0) {
                redis["db"] = parts.db;
            }
            std::cerr << "REDIS_PUBLIC_URL parsed host: " << parts.host << " port: " << (parts.port > 0 ? parts.port : redis["port"].asInt()) << std::endl;
        } else {
            std::cerr << "REDIS_PUBLIC_URL parse failed" << std::endl;
        }
    }
    overrideString(redis["host"], "REDIS_HOST");
    if (!hasEnv("REDIS_HOST")) overrideString(redis["host"], "REDISHOST");
    overrideInt(redis["port"], "REDIS_PORT");
    if (!hasEnv("REDIS_PORT")) overrideInt(redis["port"], "REDISPORT");
    overrideString(redis["username"], "REDIS_USER");
    if (!hasEnv("REDIS_USER")) overrideString(redis["username"], "REDIS_USERNAME");
    if (!hasEnv("REDIS_USER") && !hasEnv("REDIS_USERNAME")) overrideString(redis["username"], "REDISUSER");
    overrideString(redis["passwd"], "REDIS_PASS");
    if (!hasEnv("REDIS_PASS")) overrideString(redis["passwd"], "REDIS_PASSWORD");

    const std::string resolvedConfig = "/tmp/shortener_config.json";
    {
        std::ofstream output(resolvedConfig);
        output << config.toStyledString();
    }

    // DEBUG: Print resolved config to stderr
    std::cerr << "=== REDIS CONFIG ===" << std::endl;
    if (config.isMember("redis_clients") && config["redis_clients"].isArray() && config["redis_clients"].size() > 0) {
        std::cerr << "Redis host: " << config["redis_clients"][0]["host"].asString() << std::endl;
        std::cerr << "Redis port: " << config["redis_clients"][0]["port"].asInt() << std::endl;
        std::cerr << "Redis user: " << config["redis_clients"][0]["username"].asString() << std::endl;
    } else {
        std::cerr << "No redis clients configured!" << std::endl;
    }
    std::cerr << "=====================" << std::endl;

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
