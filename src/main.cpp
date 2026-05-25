#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

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

  char buffer[1024]{};
  ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

  if (bytes_read > 0)
  {
    std::string request(buffer, bytes_read);
    std::cout << "Received: " << request << "\n";

    std::string response = "+PONG\r\n";
    send(client_fd, response.c_str(), response.size(), 0);
  }

  close(client_fd);
  close(server_fd);

  std::cout << "Server stopped\n";
  return 0;
}