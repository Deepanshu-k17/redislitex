#pragma once

#include <string>
#include <vector>

std::string makeBulkString(const std::string &value);

std::vector<std::string> parseRespCommand(const std::string &request);

std::vector<std::string> parsePlainCommand(const std::string &request);