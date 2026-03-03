#pragma once

#include <cstdint>
#include <optional>
#include <string>

using namespace std;

class AuthModel {
  public:
    static bool isValidEmail(const string &email);
    static bool isStrongPassword(const string &password);

    static optional<int> registerUser(const string &email,
                                      const string &password,
                                      string *errorMessage = nullptr);

    static optional<int> authenticateUser(const string &email,
                                          const string &password);

    static string generateToken(int userId, int64_t ttlSeconds = 86400);

    static optional<int> verifyToken(const string &token,
                                     string *errorMessage = nullptr);
};

