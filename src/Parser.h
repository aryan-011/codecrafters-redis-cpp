#ifndef PARSER_H
#define PARSER_H

#include <vector>
#include <string>
#include <deque>

char upper(char c);
std::vector<std::vector<std::string>> parseResp(char *buffer);
std::vector<std::string> parseSimpleString(const std::string& raw_message);
std::vector<std::string> parseBulkString(const std::string& raw_message);
std::vector<std::string> parseArray(const std::string& raw_message);
std::string encode(std::string tmp);
std::string hexStringToBytes(const std::string& hex);

class CommandReader {
public:
    CommandReader() = default;
    void pushContent(char* buffer, int length);
    bool isCommandComplete() const;
    std::vector<std::string> getCurrentCommand();
    void popCommand();

private:
    std::vector<char> rawBuffer;
    std::deque<std::vector<std::string>> cmnds;
    std::vector<std::string> readCommand();
    
};

#endif 