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
using namespace std;


void handleClient(int client_sock){
   while(true){  
      char buffer[1024];
      int bytes_recvd = recv(client_sock, buffer, sizeof(buffer), 0);

      if (bytes_recvd < 0) {
          cerr << "Error in receiving data\n";
          close(client_sock);
      } else if (bytes_recvd == 0) {
          cout << "Client disconnected\n";
          close(client_sock);
      } else {
          cout << "Received: " << buffer << endl;
          // Sending pong 
          string response = "+PONG\r\n";
          send(client_sock, response.c_str(), response.length(), 0);
      }
    }
    close(client_sock);
}


int main(int argc, char **argv) {
  // You can use print statements as follows for debugging, they'll be visible when running tests.
  cout << "Logs from your program will appear here!\n";


  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
   cerr << "Failed to create server socket\n";
   return 1;
  }
  
  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    cerr << "setsockopt failed\n";
    return 1;
  }
  
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(6379);
  
  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
    cerr << "Failed to bind to port 6379\n";
    return 1;
  }
  
  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    cerr << "listen failed\n";
    return 1;
  }
  
  struct sockaddr_in client_addr;
  int client_addr_len = sizeof(client_addr);

  vector<thread> threads;
  
  cout << "Waiting for a client to connect...\n";
  while(1){

    int client_sock = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
    if (client_sock < 0) {
        cerr << "Failed to accept connection\n";
    }

    cout << "Client connected\n";

    thread.emplace_back(handleClient,client_sock)

  }

  for(auto &it:threads){
    thread.join();
  }

  close(server_fd);

  return 0;
}
