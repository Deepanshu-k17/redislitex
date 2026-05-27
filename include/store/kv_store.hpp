#pragma once

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

class KVStore
{
public:
  std::string set(const std::string &key, const std::string &value);
  std::optional<std::string> get(const std::string &key);
  int del(const std::string &key);
  bool exists(const std::string &key);

  int expire(const std::string &key, int seconds);
  long long ttl(const std::string &key);

  std::optional<long long> incr(const std::string &key);
  std::optional<long long> decr(const std::string &key);

  int cleanupExpiredKeys();

private:
  struct Value
  {
    std::string data;
    std::optional<std::chrono::steady_clock::time_point> expiry;
  };

  std::unordered_map<std::string, Value> store_;
  std::mutex mutex_;

  bool isExpired(const Value &value) const;
  void removeIfExpired(const std::string &key);
  bool isInteger(const std::string &value) const;
};