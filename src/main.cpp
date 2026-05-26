#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <cctype>
#include <chrono>
#include <optional>

struct Value
{
  std::string data;
  std::optional<std::chrono::steady_clock::time_point> expiry;
};

std::unordered_map<std::string, Value> store;
std::mutex store_mutex;

std::string makeBulkString(const std::string &value)
{
  return "$" + std::to_string(value.size()) + "\r\n" + value + "\r\n";
}

bool isInteger(const std::string &value)
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

bool isExpired(const Value &value)
{
  if (!value.expiry.has_value())
  {
    return false;
  }

  return std::chrono::steady_clock::now() >= value.expiry.value();
}

bool removeIfExpired(const std::string &key)
{
  auto it = store.find(key);

  if (it == store.end())
  {
    return false;
  }

  if (isExpired(it->second))
  {
    store.erase(it);
    return true;
  }

  return false;
}

std::string handleCommand(const std::string &request)
{
  std::stringstream ss(request);

  std::string command;
  ss >> command;

  if (command == "PING")
  {
    return "+PONG\r\n";
  }

  if (command == "SET")
  {
    std::string key, value;
    ss >> key >> value;

    if (key.empty() || value.empty())
    {
      return "-ERR usage: SET key value\r\n";
    }

    std::lock_guard<std::mutex> lock(store_mutex);
    store[key] = Value{value, std::nullopt};

    return "+OK\r\n";
  }

  if (command == "GET")
  {
    std::string key;
    ss >> key;

    if (key.empty())
    {
      return "-ERR usage: GET key\r\n";
    }

    std::lock_guard<std::mutex> lock(store_mutex);
    removeIfExpired(key);

    auto it = store.find(key);
    if (it == store.end())
    {
      return "$-1\r\n";
    }

    return makeBulkString(it->second.data);
  }
  if (command == "EXISTS")
  {
    std::string key;
    ss >> key;

    if (key.empty())
    {
      return "-ERR usage: EXISTS key\r\n";
    }

    std::lock_guard<std::mutex> lock(store_mutex);
    removeIfExpired(key);

    bool found = store.find(key) != store.end();
    return ":" + std::to_string(found ? 1 : 0) + "\r\n";
  }
  if (command == "EXPIRE")
  {
    std::string key;
    int seconds;

    ss >> key >> seconds;

    if (key.empty() || seconds < 0)
    {
      return "-ERR usage: EXPIRE key seconds\r\n";
    }

    std::lock_guard<std::mutex> lock(store_mutex);

    removeIfExpired(key);

    auto it = store.find(key);
    if (it == store.end())
    {
      return ":0\r\n";
    }

    it->second.expiry = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);

    return ":1\r\n";
  }
  if (command == "TTL")
  {
    std::string key;
    ss >> key;

    if (key.empty())
    {
      return "-ERR usage: TTL key\r\n";
    }

    std::lock_guard<std::mutex> lock(store_mutex);

    removeIfExpired(key);

    auto it = store.find(key);
    if (it == store.end())
    {
      return ":-2\r\n";
    }

    if (!it->second.expiry.has_value())
    {
      return ":-1\r\n";
    }

    auto now = std::chrono::steady_clock::now();
    auto remaining = std::chrono::duration_cast<std::chrono::seconds>(
                         it->second.expiry.value() - now)
                         .count();

    if (remaining < 0)
    {
      store.erase(it);
      return ":-2\r\n";
    }

    return ":" + std::to_string(remaining) + "\r\n";
  }
  if (command == "DEL")
  {
    std::string key;
    ss >> key;

    if (key.empty())
    {
      return "-ERR usage: DEL key\r\n";
    }

    std::lock_guard<std::mutex> lock(store_mutex);
    removeIfExpired(key);

    int deleted = store.erase(key);
    return ":" + std::to_string(deleted) + "\r\n";
  }
  if (command == "INCR")
  {
    std::string key;
    ss >> key;

    if (key.empty())
    {
      return "-ERR usage: INCR key\r\n";
    }

    std::lock_guard<std::mutex> lock(store_mutex);

    removeIfExpired(key);

    if (store.find(key) == store.end())
    {
      store[key] = Value{"1", std::nullopt};
      return ":1\r\n";
    }

    if (!isInteger(store[key].data))
    {
      return "-ERR value is not an integer\r\n";
    }

    long long number = std::stoll(store[key].data);
    number++;
    store[key].data = std::to_string(number);

    return ":" + std::to_string(number) + "\r\n";
  }
  if (command == "DECR")
  {
    std::string key;
    ss >> key;

    if (key.empty())
    {
      return "-ERR usage: DECR key\r\n";
    }

    std::lock_guard<std::mutex> lock(store_mutex);

    removeIfExpired(key);

    if (store.find(key) == store.end())
    {
      store[key] = Value{"-1", std::nullopt};
      return ":-1\r\n";
    }

    if (!isInteger(store[key].data))
    {
      return "-ERR value is not an integer\r\n";
    }

    long long number = std::stoll(store[key].data);
    number--;
    store[key].data = std::to_string(number);

    return ":" + std::to_string(number) + "\r\n";
  }
  return "-ERR unknown command\r\n";
}

void handleClient(int client_fd)
{
  std::cout << "Client connected\n";

  while (true)
  {
    char buffer[1024]{};
    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

    if (bytes_read == 0)
    {
      std::cout << "Client disconnected\n";
      break;
    }

    if (bytes_read < 0)
    {
      std::cerr << "Error while reading from client\n";
      break;
    }

    std::string request(buffer, bytes_read);
    std::cout << "Received: " << request;

    std::string response = handleCommand(request);
    send(client_fd, response.c_str(), response.size(), 0);
  }

  close(client_fd);
}

int main()
{
  const int port = 6379;

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == -1)
  {
    std::cerr << "Could not create server socket\n";
    return 1;
  }

  int reuse = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  sockaddr_in server_addr{};
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);

  if (bind(server_fd, reinterpret_cast<sockaddr *>(&server_addr), sizeof(server_addr)) == -1)
  {
    std::cerr << "Could not bind to port " << port << "\n";
    close(server_fd);
    return 1;
  }

  if (listen(server_fd, 5) == -1)
  {
    std::cerr << "Could not listen for connections\n";
    close(server_fd);
    return 1;
  }

  std::cout << "RedisLiteX listening on port " << port << "\n";

  while (true)
  {
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);

    int client_fd = accept(server_fd, reinterpret_cast<sockaddr *>(&client_addr), &client_len);
    if (client_fd == -1)
    {
      std::cerr << "Could not accept client\n";
      continue;
    }

    std::thread client_thread(handleClient, client_fd);
    client_thread.detach();
  }

  close(server_fd);
  return 0;
}