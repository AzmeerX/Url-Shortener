#pragma once
#include <string>
#include <optional>
#include <json/json.h>

using namespace std;

class UrlModel {
public:
    static string generateShortCode(const string& longUrl);

    static optional<string> saveUrlMapping(const string& shortCode, const string& longUrl);

    static optional<string> getOriginalUrl(const string& shortCode);

    static bool isValidUrl(const string& url);

    static optional<Json::Value> getAnalytics(const string& shortCode);
};
