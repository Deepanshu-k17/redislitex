#include "protocol/resp.hpp"

#include <sstream>

std::string makeBulkString(const std::string &value)
{
  return "$" + std::to_string(value.size()) + "\r\n" + value + "\r\n";
}

std::vector<std::string> parseRespCommand(const std::string &request)
{
  std::vector<std::string> parts;

  if (request.empty() || request[0] != '*')
  {
    return parts;
  }

  size_t pos = 1;
  size_t line_end = request.find("\r\n", pos);

  if (line_end == std::string::npos)
  {
    return parts;
  }

  int array_size = std::stoi(request.substr(pos, line_end - pos));
  pos = line_end + 2;

  for (int i = 0; i < array_size; i++)
  {
    if (pos >= request.size() || request[pos] != '$')
    {
      return {};
    }

    pos++;
    line_end = request.find("\r\n", pos);

    if (line_end == std::string::npos)
    {
      return {};
    }

    int bulk_size = std::stoi(request.substr(pos, line_end - pos));
    pos = line_end + 2;

    if (pos + bulk_size > request.size())
    {
      return {};
    }

    std::string value = request.substr(pos, bulk_size);
    parts.push_back(value);

    pos += bulk_size;

    if (pos + 2 <= request.size() && request.substr(pos, 2) == "\r\n")
    {
      pos += 2;
    }
  }

  return parts;
}

std::vector<std::string> parsePlainCommand(const std::string &request)
{
  std::stringstream ss(request);
  std::vector<std::string> parts;

  std::string word;
  while (ss >> word)
  {
    parts.push_back(word);
  }

  return parts;
}