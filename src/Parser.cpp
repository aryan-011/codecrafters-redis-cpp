#include "Parser.h"
#include <bits/stdc++.h>
#include <iostream>
#include <string>
#include <vector>
#include <deque>
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


void CommandReader::pushContent(char* buffer, int length) {
    rawBuffer.insert(rawBuffer.end(), buffer, buffer + length);
    while (!rawBuffer.empty()) {
        auto command = readCommand();
        if (!command.empty()) {
            cmnds.push_back(command);
        }
    }
}

bool CommandReader::isCommandComplete() const {
    return !cmnds.empty();
}

std::vector<std::string> CommandReader::getCurrentCommand() {
    if (cmnds.empty()) {
        return {};
    }
    auto result = cmnds.front();
    cmnds.pop_front();
    return result;
}

std::vector<std::string> CommandReader::readCommand() {
    if (rawBuffer.empty()) {
        return {};
    }

    if (rawBuffer[0] == '+') {
        auto end = std::find(rawBuffer.begin(), rawBuffer.end(), '\n');
        if (end == rawBuffer.end()) {
            return {};
        }

        std::string simpleStr(rawBuffer.begin(), end + 1);
        rawBuffer.erase(rawBuffer.begin(), end + 1);
        return parseSimpleString(simpleStr);
    }
    else if (rawBuffer[0] == '$') {
        auto start = std::find(rawBuffer.begin(), rawBuffer.end(), '\n');
        if (start == rawBuffer.end()) {
            return {};

        auto end = std::find(start + 1, rawBuffer.end(), '\n');
        if (end == rawBuffer.end()) {
            return {};
        }

        std::string bulkStr(rawBuffer.begin(), end + 1);
        rawBuffer.erase(rawBuffer.begin(), end + 1);
        return parseBulkString(bulkStr);
    }
    else if (rawBuffer[0] == '*') {
        auto start = std::find(rawBuffer.begin(), rawBuffer.end(), '\n');
        if (start == rawBuffer.end()) {
            return {};

        std::string arrStr(rawBuffer.begin(), start + 1);
        rawBuffer.erase(rawBuffer.begin(), start + 1);

        int numElems = std::stoi(std::string(rawBuffer.begin() + 1, start));
        numElems *= 2; // Expecting twice the elements due to the format

        while (numElems && !rawBuffer.empty()) {
            auto end = std::find(rawBuffer.begin(), rawBuffer.end(), '\n');
            if (end >= rawBuffer.end()) {
                return {};

            arrStr += std::string(rawBuffer.begin(), end + 1);
            rawBuffer.erase(rawBuffer.begin(), end + 1);
            numElems--;
        }

        return parseArray(arrStr);
    }
    return {};
}


// std::vector<std::vector<std::string>> parseResp(char *buffer) {
//     std::vector<std::vector<std::string>> commands;
//     std::string raw_message(buffer);
//     int i = 0;

//     while (i < raw_message.size()) {
//         if (raw_message[i] == '+') {
//             std::string simple_string;
//             while (i < raw_message.size()) {
//                 simple_string.push_back(raw_message[i]);
//                 if (raw_message.substr(i, 2) == "\r\n") {
//                     simple_string.push_back(raw_message[++i]);
//                     break;
//                 }
//                 i++;
//             }
//             commands.push_back(parseSimpleString(simple_string));
//         } else if (raw_message[i] == '$') {
//             std::string bulk_string;
//             int cnt = 0;
//             while (i < raw_message.size()) {
//                 bulk_string.push_back(raw_message[i]);
//                 if (raw_message.substr(i, 2) == "\r\n") {
//                     bulk_string.push_back(raw_message[++i]);
//                     if (++cnt == 2) break;
//                 }
//                 i++;
//             }
//             commands.push_back(parseBulkString(bulk_string));
//         } else if (raw_message[i] == '*') {
//             std::string arr;
//             while (i < raw_message.size()) {
//                 if (raw_message.substr(i, 2) == "\r\n") break;
//                 arr.push_back(raw_message[i++]);
//             }
//             int num_elements = std::stoi(arr.substr(1)) * 2;
//             arr.push_back(raw_message[i++]);
//             arr.push_back(raw_message[i++]);
//             while (i < raw_message.size() && num_elements) {
//                 if (raw_message.substr(i, 2) == "\r\n") num_elements--;
//                 arr.push_back(raw_message[i++]);
//             }
//             arr.push_back(raw_message[i]);
//             commands.push_back(parseArray(arr));
//         }
//         i++;
//     }

//     // std::cout << "Parsed " << commands.size() << " commands" << std::endl;
//     for (int i = 0; i < commands.size(); i++) {
//         std::cout << "Command " << i << ", has size of " << commands[i].size() << ": ";
//         for (int j = 0; j < commands[i].size(); j++) {
//             std::cout << commands[i][j] << ' ';
//         }
//         std::cout << std::endl;
//     }
//     return commands;
// }



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
