#pragma once

#include <atomic>

class TcpServer
{
public:
  explicit TcpServer(int port);

  void start();

private:
  int port_;
  std::atomic<bool> running_;

  void handleClient(int client_fd);
  void cleanupExpiredKeys();
};