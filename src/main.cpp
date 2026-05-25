#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>

std::unordered_map<std::string, std::string> store;

std::string makeBulkString(const std::string &value)
{
  return "$" + std::to_string(value.size()) + "\r\n" + value + "\r\n";
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

    store[key] = value;
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

    auto it = store.find(key);
    if (it == store.end())
    {
      return "$-1\r\n";
    }

    return makeBulkString(it->second);
  }

  if (command == "DEL")
  {
    std::string key;
    ss >> key;

    if (key.empty())
    {
      return "-ERR usage: DEL key\r\n";
    }

    int deleted = store.erase(key);
    return ":" + std::to_string(deleted) + "\r\n";
  }

  return "-ERR unknown command\r\n";
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

  sockaddr_in client_addr{};
  socklen_t client_len = sizeof(client_addr);

  int client_fd = accept(server_fd, reinterpret_cast<sockaddr *>(&client_addr), &client_len);
  if (client_fd == -1)
  {
    std::cerr << "Could not accept client\n";
    close(server_fd);
    return 1;
  }

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
  close(server_fd);

  std::cout << "Server stopped\n";
  return 0;
}