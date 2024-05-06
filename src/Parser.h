#ifndef PARSER_H
#define PARSER_H

#include <vector>
#include <string>

char upper(char c);
std::vector<std::string> parseResp(char *buffer);
std::string encode(std::string tmp);

#endif 