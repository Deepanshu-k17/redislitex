#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <cctype>

#include "protocol/resp.hpp"
#include "store/kv_store.hpp"

KVStore kv_store;
std::atomic<bool> server_running(true);

std::string handleCommand(const std::string &request)
{
  std::vector<std::string> parts;

  if (!request.empty() && request[0] == '*')
  {
    parts = parseRespCommand(request);
  }
  else
  {
    parts = parsePlainCommand(request);
  }

  if (parts.empty())
  {
    return "-ERR invalid command\r\n";
  }

  std::string command = parts[0];

  if (command == "PING")
  {
    return "+PONG\r\n";
  }

  if (command == "SET")
  {
    if (parts.size() < 3)
    {
      return "-ERR usage: SET key value\r\n";
    }

    kv_store.set(parts[1], parts[2]);

    return "+OK\r\n";
  }

  if (command == "GET")
  {
    if (parts.size() < 2)
    {
      return "-ERR usage: GET key\r\n";
    }

    auto value = kv_store.get(parts[1]);
    if (!value.has_value())
    {
      return "$-1\r\n";
    }

    return makeBulkString(value.value());
  }

  if (command == "EXISTS")
  {
    if (parts.size() < 2)
    {
      return "-ERR usage: EXISTS key\r\n";
    }

    bool found = kv_store.exists(parts[1]);
    return ":" + std::to_string(found ? 1 : 0) + "\r\n";
  }

  if (command == "EXPIRE")
  {
    if (parts.size() < 3)
    {
      return "-ERR usage: EXPIRE key seconds\r\n";
    }

    int seconds;

    try
    {
      seconds = std::stoi(parts[2]);
    }
    catch (...)
    {
      return "-ERR usage: EXPIRE key seconds\r\n";
    }

    if (seconds < 0)
    {
      return "-ERR usage: EXPIRE key seconds\r\n";
    }

    int result = kv_store.expire(parts[1], seconds);
    return ":" + std::to_string(result) + "\r\n";
  }

  if (command == "TTL")
  {
    if (parts.size() < 2)
    {
      return "-ERR usage: TTL key\r\n";
    }

    long long result = kv_store.ttl(parts[1]);
    return ":" + std::to_string(result) + "\r\n";
  }

  if (command == "DEL")
  {
    if (parts.size() < 2)
    {
      return "-ERR usage: DEL key\r\n";
    }

    int deleted = kv_store.del(parts[1]);
    return ":" + std::to_string(deleted) + "\r\n";
  }

  if (command == "INCR")
  {
    if (parts.size() < 2)
    {
      return "-ERR usage: INCR key\r\n";
    }

    auto result = kv_store.incr(parts[1]);
    if (!result.has_value())
    {
      return "-ERR value is not an integer\r\n";
    }

    return ":" + std::to_string(result.value()) + "\r\n";
  }

  if (command == "DECR")
  {
    if (parts.size() < 2)
    {
      return "-ERR usage: DECR key\r\n";
    }

    auto result = kv_store.decr(parts[1]);
    if (!result.has_value())
    {
      return "-ERR value is not an integer\r\n";
    }

    return ":" + std::to_string(result.value()) + "\r\n";
  }

  return "-ERR unknown command\r\n";
}
void cleanupExpiredKeys()
{
  while (server_running)
  {
    std::this_thread::sleep_for(std::chrono::seconds(5));

    int removed_count = kv_store.cleanupExpiredKeys();

    if (removed_count > 0)
    {
      std::cout << "Cleaned up " << removed_count << " expired keys\n";
    }
  }
}

void handleClient(int client_fd)
{
  std::cout << "Client connected\n";

  while (true)
  {
    char buffer[4096]{};
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
  std::thread cleanup_thread(cleanupExpiredKeys);
  cleanup_thread.detach();

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