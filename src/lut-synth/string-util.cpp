#include "lut-synth/string-util.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace lut_synth::util {

std::string trim(std::string const &s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return {};
    }
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

void trim_inplace(std::string &s) {
    size_t begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        s.clear();
        return;
    }
    size_t end = s.find_last_not_of(" \t\r\n");
    s = s.substr(begin, end - begin + 1);
}

std::vector<std::string> split_csv(std::string const &line) {
    std::vector<std::string> tokens;
    std::stringstream ss(line);
    std::string token;
    while (std::getline(ss, token, ',')) {
        tokens.push_back(trim(token));
    }
    return tokens;
}

std::vector<std::string> split(std::string const &s, char delim) {
    std::vector<std::string> tokens;
    std::stringstream ss(s);
    std::string token;
    while (std::getline(ss, token, delim)) {
        tokens.push_back(token);
    }
    return tokens;
}

std::vector<std::string> split_top_level_commas(std::string const &s) {
    std::vector<std::string> out;
    int depth = 0;
    size_t start = 0;
    for (size_t k = 0; k < s.size(); ++k) {
        char const c = s[k];
        if (c == '(') {
            ++depth;
        } else if (c == ')') {
            --depth;
        } else if (c == ',' && depth == 0) {
            out.push_back(trim(s.substr(start, k - start)));
            start = k + 1;
        }
    }
    out.push_back(trim(s.substr(start)));
    out.erase(
        std::remove_if(out.begin(), out.end(), [](std::string const &x) { return x.empty(); }),
        out.end());
    return out;
}

} // namespace lut_synth::util
