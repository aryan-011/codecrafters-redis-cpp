#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <thread>
#include <bits/stdc++.h>
#include <chrono>
#include "Parser.h"
using namespace std;

std::unordered_map<std::string, std::string> in_map;
std::unordered_map<std::string, std::chrono::time_point<std::chrono::high_resolution_clock>> expiry_map;

std::string master_replid = "8371b4fb1155b71f4a04d3e1bc3e18c4a990aeeb";
int master_repl_offset = 0;
std::string role = "master";
std::string master_host = "";
std::set<int> replica_sock;
int master_port = -1;
int port = 6379;

void handleClient(int client_sock)
{
  while (true)
  {
    char buffer[1024];
    int bytes_recvd = recv(client_sock, buffer, sizeof(buffer), 0);

    if (bytes_recvd < 0)
    {
      cerr << "Error in receiving data\n";
      close(client_sock);
    }
    else if (bytes_recvd == 0)
    {
      cout << "Client disconnected\n";
      close(client_sock);
    }
    else
    {
      cout << "Received: " << buffer << endl;
      
      vector<string> resp = parseResp(buffer);

      if (resp[0] == "ECHO")
      {
        string response = encode(resp[1]);
        send(client_sock, response.c_str(), response.length(), 0);
      }
      else if (resp[0] == "PING")
      {
        string response = "+PONG\r\n";
        send(client_sock, response.c_str(), response.length(), 0);
      }
      else if (resp[0] == "SET")
      {
        in_map[resp[1]] = resp[2];
        string response = encode("OK");
        if (resp.size() > 3)
        {
          std::transform(resp[3].begin(), resp[3].end(), resp[3].begin(), upper);
          if (resp[3] == "PX")
          {
            int expiry = stoi(resp[4]);
            std::chrono::time_point<std::chrono::high_resolution_clock> strt = std::chrono::system_clock::now();
            std::chrono::milliseconds duration(expiry);
            std::chrono::time_point<std::chrono::high_resolution_clock> expiration_time = strt + std::chrono::milliseconds(expiry);
            expiry_map[resp[1]] = expiration_time;
          }
        }

        if (role=="master"){
          send(client_sock, response.c_str(), response.length(), 0);
          for(int fd:replica_sock){
            send(fd, buffer, strlen(buffer), 0);
          }
        }
        else{
          send(client_sock, "response".c_str(), "response".length(), 0);
        }
      }
      else if (resp[0] == "GET")
      {
        string response = "";
        std::chrono::time_point<std::chrono::high_resolution_clock> strt = std::chrono::high_resolution_clock::now();
        string key = resp[1];
        if (expiry_map.count(key) != 0 && expiry_map[key] <= strt)
        {
          in_map.erase(in_map.find(key));
          expiry_map.erase(expiry_map.find(key));
        }
        else if (in_map.count(resp[1]) != 0)
        {
          response = in_map[key];
        }
        response = encode(response);
        send(client_sock, response.c_str(), response.length(), 0);
      }
      else if (resp[0] == "INFO")
      {
        std::transform(resp[1].begin(), resp[1].end(), resp[1].begin(), upper);
        if (resp[1] == "REPLICATION")
        {
          std::string response = "role:";
          response += role;
          if (role == "master")
          {
            response = response + "\n" + "master_replid:" + master_replid + "\n";
            response = response + "master_repl_offset:" + to_string(master_repl_offset) + "\n";
          }
          response = encode(response);
          send(client_sock, response.c_str(), response.length(), 0);
        }
      }
      else if (resp[0] == "REPLCONF")
      {
        std::string response = "OK";
        response=encode(response);
        send(client_sock, response.c_str(), response.length(), 0);
      }
      else if(resp[0]=="PSYNC"){
        std::string response="+FULLRESYNC ";
        response+=master_replid;
        response=response+" "+to_string(master_repl_offset);
        response+="\r\n";
        // response=encode(response);
        send(client_sock, response.c_str(), response.length(), 0);

        std::string rdb = "524544495330303131fa0972656469732d76657205372e322e30fa0a72656469732d62697473c040fa056374696d65c26d08bc65fa08757365642d6d656dc2b0c41000fa08616f662d62617365c000fff06e3bfec0ff5aa2";
        std::string res = hexStringToBytes(rdb);
        response = "$"+to_string(res.length())+"\r\n"+res;
        send(client_sock, response.c_str(), response.length(), 0);

        replica_sock.insert(client_sock);
      }
    }
  }
  close(client_sock);
}

void handleMasterConnection()
{
  int master_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (master_fd < 0)
  {
    cerr << "Failed to create slave server socket\n";
    return;
  }

  struct sockaddr_in master_server_addr;
  master_server_addr.sin_family = AF_INET;
  if (inet_pton(AF_INET, "127.0.0.1", &master_server_addr.sin_addr) <= 0)
  {
    std::cerr << "Invalid address/Address not supported\n";
    close(master_fd);
    return;
  }
  master_server_addr.sin_port = htons(master_port);

  if (connect(master_fd, (struct sockaddr *)&master_server_addr, sizeof(master_server_addr)) < 0)
  {
    std::cerr << "Connection failed\n";
    return;
  }

  char buffer[1024];
  
  std::string message =  "*1\r\n$4\r\nping\r\n";
  if (send(master_fd, message.c_str(), message.length(), 0) < 0)
  {
    std::cerr << "send FAiled\n";
    return;
  }

  int bytes_recvd = recv(master_fd, buffer, sizeof(buffer), 0);
  cout << "Received: " << buffer << endl;

  message = "*3\r\n$8\r\nREPLCONF\r\n$14\r\nlistening-port\r\n$4\r\n6380\r\n";
  if (send(master_fd, message.c_str(), message.length(), 0) < 0)
  {
    std::cerr << "send FAiled\n";
    return;
  }

  bytes_recvd = recv(master_fd, buffer, sizeof(buffer), 0);
  cout << "Received: " << buffer << endl;

  message = "*3\r\n$8\r\nREPLCONF\r\n$4\r\ncapa\r\n$6\r\npsync2\r\n";
  if (send(master_fd, message.c_str(), message.length(), 0) < 0)
  {
    std::cerr << "send FAiled\n";
    return;
  }

  bytes_recvd = recv(master_fd, buffer, sizeof(buffer), 0);
  cout << "Received: " << buffer << endl;

  message = "*3\r\n$5\r\nPSYNC\r\n$1\r\n?\r\n$2\r\n-1\r\n";
  if (send(master_fd, message.c_str(), message.length(), 0) < 0)
  {
    std::cerr << "send FAiled\n";
    return;
  }

  bytes_recvd = recv(master_fd, buffer, sizeof(buffer), 0);
  cout << "Received: " << buffer << endl;

}

int main(int argc, char **argv)
{
  // You can use print statements as follows for debugging, they'll be visible when running tests.
  cout << "Logs from your program will appear here!\n";

  for (int i = 0; i < argc; ++i)
  {
    std::string arg = argv[i];

    if (arg == "--port")
    {
      if (i + 1 < argc)
      {
        port = std::stoi(argv[i+1]);
      }
      else
      {
        std::cerr << "Error: --port requires an argument" << std::endl;
        return 1;
      }
    }
    else if (arg == "--replicaof")
    {
      if (i + 1 < argc)
      {
        role = "slave";
        std::string st = argv[i+1];
        std::stringstream replicaof(st);
        std::vector<std::string> t;
        std::string s;
        while (replicaof >> s) {
          t.push_back(s);
        }
        if (t.size() == 2) {
            master_host = t[0];
            master_port = std::stoi(t[1]);
            handleMasterConnection();
        } else {
            std::cerr << "Error: --replicaof requires 2 arguments" << std::endl;
            return 1;
        }
      }
      else
      {
        std::cerr << "Error: --port requires an argument" << std::endl;
        return 1;
      }
    }
  }

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0)
  {
    cerr << "Failed to create server socket\n";
    return 1;
  }

  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
  {
    cerr << "setsockopt failed\n";
    return 1;
  }

  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);

  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
  {
    cerr << "Failed to bind to port " << port << "\n";
    return 1;
  }

  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0)
  {
    cerr << "listen failed\n";
    return 1;
  }

  struct sockaddr_in client_addr;
  int client_addr_len = sizeof(client_addr);

  vector<thread> threads;

  cout << "Waiting for a client to connect...\n";
  while (1)
  {

    int client_sock = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_len);
    if (client_sock < 0)
    {
      cerr << "Failed to accept connection\n";
    }

    cout << "Client connected\n";

    threads.emplace_back(handleClient, client_sock);
  }

  for (auto &it : threads)
  {
    it.join();
  }

  close(server_fd);

  return 0;
}
