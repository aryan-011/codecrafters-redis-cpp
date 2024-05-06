#include "Parser.h"
#include <bits/stdc++.h>

char upper(char c) {
    return toupper(c);
}

std::vector<std::string> parseResp(char *buffer) {
    std::vector<std::string> tokens;
    std::string res(buffer);
    std::string delimiter = "\r\n";
    std::string token;
    size_t pos = res.find(delimiter);
    while (pos != std::string::npos) {
        token = res.substr(0, pos);
        tokens.push_back(token);
        res.erase(0, pos + 2);
        pos = res.find(delimiter);
    }

    if (!res.empty()) {
        tokens.push_back(res);
    }
    std::string &cmnd = tokens[0];
    std::transform(cmnd.begin(), cmnd.end(), cmnd.begin(), std::toupper);

    return tokens;
}

std::string encode(std::string tmp) {
    return "$" + std::to_string(tmp.length()) + "\r\n" + tmp + "\r\n";
}
