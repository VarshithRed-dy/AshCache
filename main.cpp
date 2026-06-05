#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <unordered_map>
#include <string>
#include <sstream>

#include <thread>
#include <mutex>

#include <queue>
#include <condition_variable>
#include <vector>

std::queue<int> client_queue;
std::mutex queue_mutex;
std::condition_variable queue_cv;

std::unordered_map<std::string, std::string> store;
std::mutex store_mutex;

std::string handle_command(std::string line){
    while(!line.empty() && (line.back() == '\r' || line.back() == '\n')){
        line.pop_back();
    }

    std::istringstream iss(line); // extracts space-separated tokens from line
    std::string command, key;
    iss >> command >> key; // extract first two tokens as command and key

    if(command == "SET"){
        std::string value;
        getline(iss, value); // extracts the rest of the line as value

        if (!value.empty() && value[0] == ' ') {
            value.erase(0, 1);
        }

        store[key] = value;
        return "+OK\n";
    }
    else if(command == "GET"){
        if(store.find(key) != store.end()){
            return store[key] + "\n";
        }else return "(nil)\n";
    }
    else if(command == "DEL"){
        return std::to_string(store.erase(key)) + "\n";
    }

    return "-ERR unknown command\n";
}

void handle_client(int client_fd) {
    char buff[1024];
        ssize_t n;
        while((n = recv(client_fd, buff, sizeof(buff), 0)) > 0){
            std::string response; //Initialised out of lock scope because I/O is not included in the lock scope
            { // Scope for lock
                std::lock_guard<std::mutex> lock(store_mutex);
                response = handle_command(std::string(buff, n));
            }
            write(client_fd, response.c_str(), response.length()); // I/O is not included in the lock scope
        }
        close(client_fd);
}

void worker_loop() {
    while(true) {
        int client_fd;

        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            
            queue_cv.wait(lock, []{
                return !client_queue.empty();
            });

            client_fd = client_queue.front();
            client_queue.pop();
        }
        handle_client(client_fd);
    }
}

int main() {
    // Create socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0); // AF_INET -> IPv4, SOCK_STREAM -> TCP, 0 -> default IPv4 + TCP protocol.

    // Set socket option
    int opt = 1; // Enable socket option
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); // SO_REUSEADDR -> Allows reuse of local addresses


    // Bind socket
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Listen on all available network interfaces
    address.sin_port = htons(6380); // Convert port number to network byte order(big -endian)

    bind(server_fd, (sockaddr*)&address, sizeof(address));
    listen(server_fd, 10); // 10 -> Maximum number of pending connections

    std::cout<<"Listening on 6380...\n";
    
    const int workers = 4;
    std::vector<std::thread> worker_threads;
    worker_threads.reserve(workers);

    for(int i = 0; i < workers; i++){
        worker_threads.emplace_back(worker_loop);
    }

    while(true){
        int client_fd = accept(server_fd, nullptr, nullptr);
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            client_queue.push(client_fd);
        }
        queue_cv.notify_one();
    }

    close(server_fd);
    std::cout<<"Server stopped.\n";
    return 0;
}