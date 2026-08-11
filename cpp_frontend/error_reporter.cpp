#include "error_reporter.h"
#include <fstream>
#include <sstream>
#include <vector>

std::string ErrorReporter::formatError(const std::string& msg, const std::string& file, int line, int col, bool use_color) {
    std::string result = (use_color ? "\n\033[1;31m[ALU CXX] Compile Error in \033[0m" : "\n[ALU CXX] Compile Error in ") + file + ":" + std::to_string(line) + ":" + std::to_string(col) + "\n";
    result += (use_color ? "\033[1;37m" + msg + "\033[0m\n\n" : msg + "\n\n");
    
    std::ifstream f(file);
    if (f.is_open()) {
        std::string currentLine;
        int currentLineNum = 1;
        while (std::getline(f, currentLine)) {
            if (currentLineNum == line) {
                result += "  " + std::to_string(line) + " | " + currentLine + "\n";
                result += "  " + std::string(std::to_string(line).length(), ' ') + " | ";
                for (int i = 1; i < col && i <= currentLine.length(); ++i) {
                    result += " ";
                }
                result += (use_color ? "\033[1;31m^~~~\033[0m\n" : "^~~~\n");
                break;
            }
            currentLineNum++;
        }
    }
    return result;
}
