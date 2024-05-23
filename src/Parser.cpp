#include "Parser.h"
#include <bits/stdc++.h>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>

const std::string null_bulk_string = "$-1\r\n";

char upper(char c) {
    return toupper(c);
}


bool startsWithSpecialCharacter(const std::string &str) {
    return str.empty() || str[0] == '+' || str[0] == '$' || str[0] == '*' || str[0] == ':';
}

std::vector<std::string> parseSimpleString(const std::string &simple_string) {
    return {simple_string.substr(1, simple_string.size() - 3)};
}

std::vector<std::string> parseBulkString(const std::string &bulk_string) {
    // Format: $<length>\r\n<data>\r\n
    size_t start = bulk_string.find("\r\n");
    size_t end = bulk_string.find("\r\n", start + 2);
    return {bulk_string.substr(start + 2, end - start - 2)};
}

std::vector<std::string> parseArray(const std::string &arr) {
    std::vector<std::string> elements;
    size_t start = arr.find("\r\n") + 2;
    std::string remaining = arr.substr(start);

    while (!remaining.empty()) {
        if (remaining[0] == '$') {
            size_t end = remaining.find("\r\n", 1);
            int length = std::stoi(remaining.substr(1, end - 1));
            start = end + 2;
            end = start + length;
            elements.push_back(remaining.substr(start, length));
            remaining = remaining.substr(end + 2);
        } else {
            break;
        }
    }
    return elements;
}


std::vector<std::vector<std::string>> parseResp(char* buffer) {
    std::string str_buffer(buffer);
    std::vector<std::vector<std::string>> commands;
    std::istringstream iss(str_buffer);
    std::string token;
    std::string remaining_data;

    while (std::getline(iss, token, '\n')) {
        remaining_data += token + "\n";

        while (startsWithSpecialCharacter(remaining_data)) {
            try {
                if (remaining_data[0] == '+') {
                    commands.push_back(parseSimpleString(remaining_data));
                    remaining_data = remaining_data.substr(commands.back().front().size() + 3);
                } else if (remaining_data[0] == '$') {
                    auto bulk_strings = parseBulkString(remaining_data);
                    commands.push_back(bulk_strings);
                    remaining_data = remaining_data.substr(bulk_strings.front().size() + remaining_data.find("\r\n", 1) + 4);
                } else if (remaining_data[0] == '*') {
                    size_t end_pos = remaining_data.find("\r\n");
                    std::string arr_header = remaining_data.substr(0, end_pos + 2);
                    remaining_data = remaining_data.substr(end_pos + 2);
                    std::string arr = arr_header;

                    while (!remaining_data.empty()) {
                        end_pos = remaining_data.find("\r\n");
                        if (end_pos == std::string::npos) {
                            arr += remaining_data;
                            remaining_data.clear();
                            break;
                        }
                        arr += remaining_data.substr(0, end_pos + 2);
                        remaining_data = remaining_data.substr(end_pos + 2);
                    }

                    commands.push_back(parseArray(arr));
                }
            } catch (const std::exception& e) {
                std::cerr << "Error parsing response: " << e.what() << std::endl;
                commands.clear();
                return commands;
            }
        }
    }

    std::cout << "Parsed " << commands.size() << " commands" << std::endl;
    for (int i = 0; i < commands.size(); i++) {
        std::cout << "Command " << i << ", has size of " << commands[i].size() << ": ";
        for (int j = 0; j < commands[i].size(); j++) {
            std::cout << commands[i][j] << ' ';
        }
        std::cout << std::endl;
    }

    return commands;
}



// std::vector<std::string> parseResp(char *buffer) {
//     std::vector<std::string> tokens;
//     std::string res(buffer);
//     std::string delimiter = "\r\n";
//     std::string token;
//     size_t pos = res.find(delimiter);
//     while (pos != std::string::npos) {
//         token = res.substr(0, pos);
//         tokens.push_back(token);
//         res.erase(0, pos + 2);
//         pos = res.find(delimiter);
//     }

//     if (!res.empty()) {
//         tokens.push_back(res);
//     }
//     std::string &cmnd = tokens[0];
//     std::transform(cmnd.begin(), cmnd.end(), cmnd.begin(), upper);

//     tokens.erase(
//         std::remove_if(tokens.begin(), tokens.end(), startsWithSpecialCharacter ),
//         tokens.end()
//     );

//     return tokens;
// }

std::string encode(std::string tmp) {
    if (tmp.empty()) {
        return "$-1\r\n"; 
    } else {
        return "$" + std::to_string(tmp.length()) + "\r\n" + tmp + "\r\n";
    }
}

std::string hexStringToBytes(const std::string& hex) {
    std::string result;
    for (size_t i = 0; i < hex.length(); i += 2) {
        unsigned int byte = std::stoi(hex.substr(i, 2), nullptr, 16);
        result.push_back(static_cast<char>(byte));
    }

    return result;
}

