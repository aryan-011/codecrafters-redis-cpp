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
#include <spdlog/spdlog.h>

using namespace std;

// Use a hash table for storing key-value pairs
std::unordered_map<std::string, std::string> kv_store;
std::unordered_map<std::string, std::chrono::time_point<std::chrono::high_resolution_clock>> expiry_map;

std::string master_replid = "8371b4fb1155b71f4a04d3e1bc3e18c4a990aeeb";
int master_repl_offset = 0;
std::string role = "master";
std::string master_host = "";
std::set<int> replica_socks;
int master_port = -1;
int port = 6379;
bool handshake_complete = false;

// Connection pool for communicating with the master
std::vector<int> master_conn_pool;
std::mutex master_conn_mutex;

// Thread pool for handling client connections
std::vector<std::thread> thread_pool;
std::queue<int> client_sock_queue;
std::mutex client_sock_mutex;
std::condition_variable client_sock_cv;

// Command batching
std::vector<std::vector<std::string>> command_batch;
std::mutex command_batch_mutex;

void handleCommand(std::vector<std::string> command_vec, int client_sock) {
    std::string command = command_vec[0];
    std::transform(command.begin(), command.end(), command.begin(), ::toupper);

    if (command == "ECHO" && command_vec.size() > 1) {
        std::string response = encode(command_vec[1]);
        send(client_sock, response.c_str(), response.length(), 0);
    } else if (command == "PING") {
        std::string response = "+PONG\r\n";
        send(client_sock, response.c_str(), response.length(), 0);
    } else if (command == "SET" && command_vec.size() > 2) {
        kv_store[command_vec[1]] = command_vec[2];
        std::string response = encode("OK");

        if (command_vec.size() > 3) {
            std::string option = command_vec[3];
            std::transform(option.begin(), option.end(), option.begin(), ::toupper);
            if (option == "PX" && command_vec.size() > 4) {
                int expiry = std::stoi(command_vec[4]);
                auto now = std::chrono::system_clock::now();
                auto expiration_time = now + std::chrono::milliseconds(expiry);
                expiry_map[command_vec[1]] = expiration_time;
            }
        }

        if (role == "master") {
            send(client_sock, response.c_str(), response.length(), 0);
            command_batch_mutex.lock();
            command_batch.push_back(command_vec);
            command_batch_mutex.unlock();
        } else {
            send(client_sock, encode("response").c_str(), encode("response").length(), 0);
        }
    } else if (command == "GET" && command_vec.size() > 1) {
        std::string response;
        auto now = std::chrono::high_resolution_clock::now();
        std::string key = command_vec[1];

        if (expiry_map.count(key) != 0 && expiry_map[key] <= now) {
            kv_store.erase(key);
            expiry_map.erase(key);
        } else if (kv_store.count(key) != 0) {
            response = kv_store[key];
        }

        response = encode(response);
        send(client_sock, response.c_str(), response.length(), 0);
    } else if (command == "INFO" && command_vec.size() > 1) {
        std::string option = command_vec[1];
        std::transform(option.begin(), option.end(), option.begin(), ::toupper);

        if (option == "REPLICATION") {
            std::string response = "role:" + role;
            if (role == "master") {
                response += "\nmaster_replid:" + master_replid + "\n";
                response += "master_repl_offset:" + std::to_string(master_repl_offset) + "\n";
            }
            response = encode(response);
            send(client_sock, response.c_str(), response.length(), 0);
        }
    } else if (command == "REPLCONF") {
        if (command_vec.size() >= 3) {
            if (role != "master" && handshake_complete) {
                std::string response = "*3\r\n$8\r\nREPLCONF\r\n$3\r\nACK\r\n$1\r\n0\r\n";
                send(client_sock, response.c_str(), response.length(), 0);
            } else {
                std::string response = encode("OK");
                send(client_sock, response.c_str(), response.length(), 0);
                replica_socks.insert(client_sock);
            }
        }
    } else if (command == "PSYNC") {
        std::string response = "+FULLRESYNC " + master_replid + " " + std::to_string(master_repl_offset) + "\r\n";
        send(client_sock, response.c_str(), response.length(), 0);

        // Generate RDB file dynamically
        // std::string rdb = generateRDBFile();
        std::string rdb = "524544495330303131fa0972656469732d76657205372e322e30fa0a72656469732d62697473c040fa056374696d65c26d08bc65fa08757365642d6d656dc2b0c41000fa08616f662d62617365c000fff06e3bfec0ff5aa2";
        std::string res = hexStringToBytes(rdb);
        response = "$" + std::to_string(res.length()) + "\r\n" + res;
        send(client_sock, response.c_str(), response.length(), 0);

        replica_socks.insert(client_sock);
    }
}

void handleClient(int client_sock) {
    if (role != "master") {
        spdlog::info("Slve Here");
    }
    spdlog::info("Here");

    while (true) {
        if (role != "master") {
            spdlog::info("Slve Here2");
        }

        char buffer[1024];
        int bytes_recvd = recv(client_sock, buffer, sizeof(buffer), 0);

        if (bytes_recvd < 0) {
            spdlog::error("Error in receiving data");
            close(client_sock);
            break;
        } else if (bytes_recvd == 0) {
            spdlog::info("Client disconnected");
            close(client_sock);
            break;
        } else {
            spdlog::info("Received: {}", buffer);

            CommandReader commandReader;
            commandReader.pushContent(buffer, sizeof(buffer) - 1);

            while (commandReader.isCommandComplete()) {
                std::vector<std::string> command_vec = commandReader.getCurrentCommand();
                if (!command_vec.empty()) {
                    handleCommand(command_vec, client_sock);
                }
                commandReader.popCommand();
            }
        }
    }
}

void workerThread() {
    while (true) {
        std::unique_lock<std::mutex> lock(client_sock_mutex);
        client_sock_cv.wait(lock, [&]() { return !client_sock_queue.empty(); });

        int client_sock = client_sock_queue.front();
        client_sock_queue.pop();
        lock.unlock();

        handleClient(client_sock);
    }
}

void handleMasterConnection() {
    if (role == "slave") {
        int master_fd = -1;
        {
            std::unique_lock<std::mutex> lock(master_conn_mutex);
            if (!master_conn_pool.empty()) {
                master_fd = master_conn_pool.back();
                master_conn_pool.pop_back();
            }
        }

        if (master_fd == -1) {
            master_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (master_fd < 0) {
                spdlog::error("Failed to create slave server socket");
                return;
            }

            struct sockaddr_in master_server_addr;
            master_server_addr.sin_family = AF_INET;
            if (inet_pton(AF_INET, "127.0.0.1", &master_server_addr.sin_addr) <= 0) {
                spdlog::error("Invalid address/Address not supported");
                close(master_fd);
                return;
            }
            master_server_addr.sin_port = htons(master_port);

            if (connect(master_fd, (struct sockaddr *)&master_server_addr, sizeof(master_server_addr)) < 0) {
                spdlog::error("Connection failed");
                close(master_fd);
                return;
            }
        }

        char buffer[2024];

        std::string message = "*1\r\n$4\r\nping\r\n";
        if (send(master_fd, message.c_str(), message.length(), 0) < 0) {
            spdlog::error("send Failed");
            close(master_fd);
            return;
        }

        int bytes_recvd = recv(master_fd, buffer, sizeof(buffer), 0);
        spdlog::info("Received: {}", buffer);

        message = "*3\r\n$8\r\nREPLCONF\r\n$14\r\nlistening-port\r\n$4\r\n6380\r\n";
        if (send(master_fd, message.c_str(), message.length(), 0) < 0) {
            spdlog::error("send Failed");
            close(master_fd);
            return;
        }

        bytes_recvd = recv(master_fd, buffer, sizeof(buffer), 0);
        spdlog::info("Received: {}", buffer);

        message = "*3\r\n$8\r\nREPLCONF\r\n$4\r\ncapa\r\n$6\r\npsync2\r\n";
        if (send(master_fd, message.c_str(), message.length(), 0) < 0) {
            spdlog::error("send Failed");
            close(master_fd);
            return;
        }

        bytes_recvd = recv(master_fd, buffer, sizeof(buffer), 0);

        message = "*3\r\n$5\r\nPSYNC\r\n$1\r\n?\r\n$2\r\n-1\r\n";
        if (send(master_fd, message.c_str(), message.length(), 0) < 0) {
            spdlog::error("send Failed");
            close(master_fd);
            return;
        }

        handshake_complete = true;

        std::thread t(handleClient, master_fd);
        t.detach();

        {
            std::unique_lock<std::mutex> lock(master_conn_mutex);
            master_conn_pool.push_back(master_fd);
        }
    }
}

int main(int argc, char **argv) {
    spdlog::info("Logs from your program will appear here!");

    for (int i = 0; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--port") {
            if (i + 1 < argc) {
                port = std::stoi(argv[i + 1]);
            } else {
                spdlog::error("Error: --port requires an argument");
                return 1;
            }
        } else if (arg == "--replicaof") {
            if (i + 1 < argc) {
                role = "slave";
                std::string st = argv[i + 1];
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
                    spdlog::error("Error: --replicaof requires 2 arguments");
                    return 1;
                }
            } else {
                spdlog::error("Error: --port requires an argument");
                return 1;
            }
        }
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        spdlog::error("Failed to create server socket");
        return 1;
    }

    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        spdlog::error("setsockopt failed");
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
        spdlog::error("Failed to bind to port {}", port);
    return 1;
    }

    int connection_backlog = 5;
    if (listen(server_fd, connection_backlog) != 0) {
        spdlog::error("listen failed");
        return 1;
    }

    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);

    // Initialize the thread pool
    const unsigned int num_threads = std::thread::hardware_concurrency();
    for (unsigned int i = 0; i < num_threads; ++i) {
        thread_pool.emplace_back(workerThread);
    }

    spdlog::info("Waiting for a client to connect...");
    while (true) {
        int client_sock = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_len);
        if (client_sock < 0) {
            spdlog::error("Failed to accept connection");
            continue;
        }

        spdlog::info("Client connected");

        std::unique_lock<std::mutex> lock(client_sock_mutex);
        client_sock_queue.push(client_sock);
        client_sock_cv.notify_one();
    }

    for (auto &t : thread_pool) {
        t.join();
    }

    close(server_fd);

    return 0;
}

