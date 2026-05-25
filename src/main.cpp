#include <iostream>
#include <string>
#include <unordered_map>
#include <sstream>

int main()
{
  std::unordered_map<std::string, std::string> store;

  std::cout << "RedisLiteX fake server started. Type commands.\n";
  std::cout << "Available commands: PING, SET key value, GET key, DEL key, EXIT\n";

  std::string line;

  while (true)
  {
    std::cout << "> ";
    std::getline(std::cin, line);

    std::stringstream ss(line);
    std::string command;
    ss >> command;

    if (command == "PING")
    {
      std::cout << "PONG\n";
    }
    else if (command == "SET")
    {
      std::string key, value;
      ss >> key >> value;

      if (key.empty() || value.empty())
      {
        std::cout << "ERR usage: SET key value\n";
      }
      else
      {
        store[key] = value;
        std::cout << "OK\n";
      }
    }
    else if (command == "GET")
    {
      std::string key;
      ss >> key;

      if (store.find(key) == store.end())
      {
        std::cout << "(nil)\n";
      }
      else
      {
        std::cout << store[key] << "\n";
      }
    }
    else if (command == "DEL")
    {
      std::string key;
      ss >> key;

      int deleted = store.erase(key);
      std::cout << deleted << "\n";
    }
    else if (command == "EXIT")
    {
      std::cout << "Bye\n";
      break;
    }
    else
    {
      std::cout << "ERR unknown command\n";
    }
  }

  return 0;
}