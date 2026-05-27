#include <arpa/inet.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

std::string makeRespCommand(const std::vector<std::string> &parts)
{
  std::string command = "*" + std::to_string(parts.size()) + "\r\n";

  for (const std::string &part : parts)
  {
    command += "$" + std::to_string(part.size()) + "\r\n";
    command += part + "\r\n";
  }

  return command;
}

int connectToServer(const std::string &host, int port)
{
  int client_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (client_fd == -1)
  {
    return -1;
  }

  sockaddr_in server_addr{};
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);

  if (inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr) <= 0)
  {
    close(client_fd);
    return -1;
  }

  if (connect(client_fd, reinterpret_cast<sockaddr *>(&server_addr), sizeof(server_addr)) == -1)
  {
    close(client_fd);
    return -1;
  }

  return client_fd;
}

bool sendCommand(int client_fd, const std::string &command)
{
  ssize_t bytes_sent = send(client_fd, command.c_str(), command.size(), 0);
  return bytes_sent == (ssize_t)command.size();
}

bool readResponse(int client_fd)
{
  char buffer[4096]{};
  ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
  return bytes_read > 0;
}

void runWorker(
    int worker_id,
    int requests_per_worker,
    std::vector<long long> &latencies_us)
{
  int client_fd = connectToServer("127.0.0.1", 6379);

  if (client_fd == -1)
  {
    std::cerr << "Worker " << worker_id << " could not connect to server\n";
    return;
  }

  for (int i = 0; i < requests_per_worker; i++)
  {
    std::string key = "key:" + std::to_string(worker_id) + ":" + std::to_string(i);
    std::string value = "value:" + std::to_string(i);

    std::string command = makeRespCommand({"SET", key, value});

    auto start = std::chrono::high_resolution_clock::now();

    bool sent = sendCommand(client_fd, command);
    bool received = false;

    if (sent)
    {
      received = readResponse(client_fd);
    }

    auto end = std::chrono::high_resolution_clock::now();

    if (sent && received)
    {
      auto latency = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
      latencies_us.push_back(latency);
    }
  }

  close(client_fd);
}

long long percentile(const std::vector<long long> &values, double p)
{
  if (values.empty())
  {
    return 0;
  }

  size_t index = (size_t)((p / 100.0) * (values.size() - 1));
  return values[index];
}

int main(int argc, char *argv[])
{
  int clients = 10;
  int total_requests = 10000;

  if (argc >= 2)
  {
    clients = std::stoi(argv[1]);
  }

  if (argc >= 3)
  {
    total_requests = std::stoi(argv[2]);
  }

  if (clients <= 0 || total_requests <= 0)
  {
    std::cerr << "Usage: ./redislitex_bench [clients] [total_requests]\n";
    return 1;
  }

  int requests_per_worker = total_requests / clients;

  std::vector<std::thread> workers;
  std::vector<std::vector<long long>> all_latencies(clients);

  auto bench_start = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < clients; i++)
  {
    workers.emplace_back(runWorker, i, requests_per_worker, std::ref(all_latencies[i]));
  }

  for (auto &worker : workers)
  {
    worker.join();
  }

  auto bench_end = std::chrono::high_resolution_clock::now();

  std::vector<long long> latencies_us;

  for (const auto &worker_latencies : all_latencies)
  {
    for (long long latency : worker_latencies)
    {
      latencies_us.push_back(latency);
    }
  }

  std::sort(latencies_us.begin(), latencies_us.end());

  double total_time_sec = std::chrono::duration<double>(bench_end - bench_start).count();
  int completed_requests = (int)latencies_us.size();
  double throughput = completed_requests / total_time_sec;

  std::cout << "RedisLiteX Benchmark\n";
  std::cout << "Clients: " << clients << "\n";
  std::cout << "Requested operations: " << total_requests << "\n";
  std::cout << "Completed operations: " << completed_requests << "\n";
  std::cout << "Total time: " << total_time_sec << " sec\n";
  std::cout << "Throughput: " << throughput << " ops/sec\n";

  std::cout << "p50 latency: " << percentile(latencies_us, 50) << " us\n";
  std::cout << "p95 latency: " << percentile(latencies_us, 95) << " us\n";
  std::cout << "p99 latency: " << percentile(latencies_us, 99) << " us\n";

  return 0;
}