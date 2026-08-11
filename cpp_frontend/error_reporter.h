#pragma once
#include <string>

class ErrorReporter {
public:
    static std::string formatError(const std::string& msg, const std::string& file, int line, int col, bool use_color = true);
};
