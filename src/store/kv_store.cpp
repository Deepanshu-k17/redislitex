#include "store/kv_store.hpp"

#include <cctype>

std::string KVStore::set(const std::string &key, const std::string &value)
{
  std::lock_guard<std::mutex> lock(mutex_);

  store_[key] = Value{value, std::nullopt};

  return "OK";
}

std::optional<std::string> KVStore::get(const std::string &key)
{
  std::lock_guard<std::mutex> lock(mutex_);

  removeIfExpired(key);

  auto it = store_.find(key);
  if (it == store_.end())
  {
    return std::nullopt;
  }

  return it->second.data;
}

int KVStore::del(const std::string &key)
{
  std::lock_guard<std::mutex> lock(mutex_);

  removeIfExpired(key);

  return store_.erase(key);
}

bool KVStore::exists(const std::string &key)
{
  std::lock_guard<std::mutex> lock(mutex_);

  removeIfExpired(key);

  return store_.find(key) != store_.end();
}

int KVStore::expire(const std::string &key, int seconds)
{
  std::lock_guard<std::mutex> lock(mutex_);

  removeIfExpired(key);

  auto it = store_.find(key);
  if (it == store_.end())
  {
    return 0;
  }

  it->second.expiry = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);

  return 1;
}

long long KVStore::ttl(const std::string &key)
{
  std::lock_guard<std::mutex> lock(mutex_);

  removeIfExpired(key);

  auto it = store_.find(key);
  if (it == store_.end())
  {
    return -2;
  }

  if (!it->second.expiry.has_value())
  {
    return -1;
  }

  auto now = std::chrono::steady_clock::now();
  auto remaining = std::chrono::duration_cast<std::chrono::seconds>(
                       it->second.expiry.value() - now)
                       .count();

  if (remaining < 0)
  {
    store_.erase(it);
    return -2;
  }

  return remaining;
}

std::optional<long long> KVStore::incr(const std::string &key)
{
  std::lock_guard<std::mutex> lock(mutex_);

  removeIfExpired(key);

  auto it = store_.find(key);
  if (it == store_.end())
  {
    store_[key] = Value{"1", std::nullopt};
    return 1;
  }

  if (!isInteger(it->second.data))
  {
    return std::nullopt;
  }

  long long number = std::stoll(it->second.data);
  number++;

  it->second.data = std::to_string(number);

  return number;
}

std::optional<long long> KVStore::decr(const std::string &key)
{
  std::lock_guard<std::mutex> lock(mutex_);

  removeIfExpired(key);

  auto it = store_.find(key);
  if (it == store_.end())
  {
    store_[key] = Value{"-1", std::nullopt};
    return -1;
  }

  if (!isInteger(it->second.data))
  {
    return std::nullopt;
  }

  long long number = std::stoll(it->second.data);
  number--;

  it->second.data = std::to_string(number);

  return number;
}

int KVStore::cleanupExpiredKeys()
{
  std::lock_guard<std::mutex> lock(mutex_);

  int removed_count = 0;

  for (auto it = store_.begin(); it != store_.end();)
  {
    if (isExpired(it->second))
    {
      it = store_.erase(it);
      removed_count++;
    }
    else
    {
      ++it;
    }
  }

  return removed_count;
}

bool KVStore::isExpired(const Value &value) const
{
  if (!value.expiry.has_value())
  {
    return false;
  }

  return std::chrono::steady_clock::now() >= value.expiry.value();
}

void KVStore::removeIfExpired(const std::string &key)
{
  auto it = store_.find(key);

  if (it == store_.end())
  {
    return;
  }

  if (isExpired(it->second))
  {
    store_.erase(it);
  }
}

bool KVStore::isInteger(const std::string &value) const
{
  if (value.empty())
  {
    return false;
  }

  int start = 0;

  if (value[0] == '-')
  {
    if (value.size() == 1)
    {
      return false;
    }

    start = 1;
  }

  for (int i = start; i < (int)value.size(); i++)
  {
    if (!std::isdigit(value[i]))
    {
      return false;
    }
  }

  return true;
}