#include "AuthModel.h"

#include <chrono>
#include <regex>
#include <vector>

#include <drogon/drogon.h>

using namespace drogon;
using namespace std;

namespace {
const string kTokenType = "JWT";
const string kTokenAlg = "SHA256";

string getJwtSecret() {
    const char *envSecret = std::getenv("JWT_SECRET");
    if (envSecret && string(envSecret).size() >= 16) {
        return string(envSecret);
    }
    return "change-this-in-production-very-secret-key";
}

string base64Encode(const string &input) {
    return utils::base64Encode(input, false, true);
}

optional<string> base64Decode(const string &input) {
    try {
        return utils::base64Decode(input);
    } catch (const exception &) {
        return nullopt;
    }
}

vector<string> splitToken(const string &token) {
    return utils::splitString(token, ".", false);
}

string signInput(const string &signingInput) {
    const string material = signingInput + "." + getJwtSecret();
    const string digest = utils::getSha256(material);
    return base64Encode(digest);
}

string generatePasswordHash(const string &password, const string &salt) {
    return utils::getSha256(salt + ":" + password);
}

optional<pair<string, string>> splitStoredPassword(const string &stored) {
    const auto pos = stored.find('$');
    if (pos == string::npos) {
        return nullopt;
    }
    return make_pair(stored.substr(0, pos), stored.substr(pos + 1));
}
}  // namespace

bool AuthModel::isValidEmail(const string &email) {
    if (email.empty() || email.size() > 254) {
        return false;
    }

    static const regex emailRegex(
        R"(^[A-Za-z0-9._%+\-]+@[A-Za-z0-9.\-]+\.[A-Za-z]{2,}$)");
    return regex_match(email, emailRegex);
}

bool AuthModel::isStrongPassword(const string &password) {
    return password.size() >= 8 && password.size() <= 128;
}

optional<int> AuthModel::registerUser(const string &email,
                                      const string &password,
                                      string *errorMessage) {
    try {
        auto client = app().getDbClient("default");
        const string salt = utils::genRandomString(16);
        const string hash = generatePasswordHash(password, salt);
        const string storedPassword = salt + "$" + hash;

        auto result = client->execSqlSync(
            "INSERT INTO users(email, password_hash) VALUES($1, $2) RETURNING id",
            email,
            storedPassword);

        if (result.empty()) {
            if (errorMessage) {
                *errorMessage = "Failed to create user";
            }
            return nullopt;
        }

        return result[0]["id"].as<int>();
    } catch (const exception &e) {
        const string err = e.what();
        if (errorMessage) {
            if (err.find("users_email_key") != string::npos ||
                err.find("duplicate key value") != string::npos) {
                *errorMessage = "User already exists";
            } else {
                *errorMessage = "Registration failed";
            }
        }
        LOG_ERROR << "Register failed: " << err;
        return nullopt;
    }
}

optional<int> AuthModel::authenticateUser(const string &email,
                                          const string &password) {
    try {
        auto client = app().getDbClient("default");
        auto result = client->execSqlSync(
            "SELECT id, password_hash FROM users WHERE email=$1 LIMIT 1", email);

        if (result.empty()) {
            return nullopt;
        }

        const int userId = result[0]["id"].as<int>();
        const string stored = result[0]["password_hash"].as<string>();
        auto parsed = splitStoredPassword(stored);
        if (!parsed) {
            LOG_ERROR << "Corrupted password hash format for user: " << userId;
            return nullopt;
        }

        const string recomputed = generatePasswordHash(password, parsed->first);
        if (recomputed != parsed->second) {
            return nullopt;
        }

        return userId;
    } catch (const exception &e) {
        LOG_ERROR << "Authenticate failed: " << e.what();
        return nullopt;
    }
}

string AuthModel::generateToken(int userId, int64_t ttlSeconds) {
    const auto nowSeconds = chrono::duration_cast<chrono::seconds>(
                                chrono::system_clock::now().time_since_epoch())
                                .count();
    const auto expSeconds = nowSeconds + ttlSeconds;

    Json::Value header;
    header["typ"] = kTokenType;
    header["alg"] = kTokenAlg;

    Json::Value payload;
    payload["sub"] = userId;
    payload["iat"] = static_cast<Json::Int64>(nowSeconds);
    payload["exp"] = static_cast<Json::Int64>(expSeconds);

    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    const string encodedHeader = base64Encode(Json::writeString(writer, header));
    const string encodedPayload = base64Encode(Json::writeString(writer, payload));
    const string signingInput = encodedHeader + "." + encodedPayload;
    const string signature = signInput(signingInput);

    return signingInput + "." + signature;
}

optional<int> AuthModel::verifyToken(const string &token, string *errorMessage) {
    const auto parts = splitToken(token);
    if (parts.size() != 3) {
        if (errorMessage) {
            *errorMessage = "Malformed token";
        }
        return nullopt;
    }

    const string signingInput = parts[0] + "." + parts[1];
    const string expectedSignature = signInput(signingInput);
    if (expectedSignature != parts[2]) {
        if (errorMessage) {
            *errorMessage = "Invalid token signature";
        }
        return nullopt;
    }

    auto decodedPayload = base64Decode(parts[1]);
    if (!decodedPayload) {
        if (errorMessage) {
            *errorMessage = "Invalid token payload";
        }
        return nullopt;
    }

    Json::CharReaderBuilder readerBuilder;
    Json::Value payload;
    string parseErrors;
    unique_ptr<Json::CharReader> reader(readerBuilder.newCharReader());
    const bool ok = reader->parse(decodedPayload->data(),
                                  decodedPayload->data() + decodedPayload->size(),
                                  &payload,
                                  &parseErrors);
    if (!ok || !payload.isObject() || !payload["sub"].isInt() ||
        !payload["exp"].isInt64()) {
        if (errorMessage) {
            *errorMessage = "Invalid token claims";
        }
        return nullopt;
    }

    const auto nowSeconds = chrono::duration_cast<chrono::seconds>(
                                chrono::system_clock::now().time_since_epoch())
                                .count();
    if (payload["exp"].asInt64() < nowSeconds) {
        if (errorMessage) {
            *errorMessage = "Token expired";
        }
        return nullopt;
    }

    return payload["sub"].asInt();
}

