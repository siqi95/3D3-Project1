#include "HttpRequest.h"
#include "HttpResponse.h"
#include "SimpleHttpServer.h"


#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <future>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

std::vector<sockaddr> get_ip_address(const std::string &hostname, const unsigned short port);
std::string ip_to_string(const sockaddr &address);
void print_usage();

int main(int argc, char **argv) {
    if (argc < 4) {
        print_usage();
        return 1;
    }

    std::string hostname = argv[1];
    unsigned short port;
    std::string root = argv[3];
    try {
        port = std::stoi(argv[2]);
        if (std::stoi(argv[2]) > std::numeric_limits<unsigned short>::max()) {
            throw std::out_of_range("");
        }
    } catch (const std::logic_error&) {
        print_usage();
        std::cerr << "port must be an integer between 0 and 65535" << std::endl;
        return 1;
    }

    SimpleHttpServer server(hostname, port, root);

    // Get addresses to listen on
    std::vector<sockaddr> addresses;
    try {
        addresses = get_ip_address(hostname, port);
    } catch (const std::runtime_error &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    // Start listening on each address
    int max_listen_socket_fd = 0;
    fd_set listen_sockets;
    FD_ZERO(&listen_sockets);
    for (auto it = addresses.begin(); it != addresses.end(); it++) {
        const sockaddr &addr = *it;
        int sock = 0;
        int result;

        // Create socket
        sock = socket(addr.sa_family, SOCK_STREAM, 0);
        if (sock == -1) {
            std::cerr << "Error creating socket on " << ip_to_string(addr) << std::endl;
            continue;
        }
        // Set socket to be immediately reusable
        int truthy = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &truthy, sizeof(truthy));

        // Bind socket to address
        result = bind(sock, &*it, sizeof(*it));
        if (result == -1) {
            std::cerr << "Error binding socket on " << ip_to_string(addr) << std::endl;
            close(sock);
            continue;
        }

        // Listen on socket
        result = listen(sock, 16);
        if (result == -1) {
            std::cerr << "Error listenting on " << ip_to_string(addr) << std::endl;
            close(sock);
            continue;
        }

        max_listen_socket_fd = std::max(max_listen_socket_fd, sock);
        FD_SET(sock, &listen_sockets);
        std::cout << "Listening on " << ip_to_string(addr) << " on port " << port << std::endl;
    }
    if (max_listen_socket_fd == 0) {
        std::cerr << "Error: No addresses to listen_sockets on" << std::endl;
        std::abort();
    }

    // Wait for connections and handle them
    std::vector<std::future<void>> connections;
    while (true) {
        timeval timer = {};
        timer.tv_sec = 0;
        timer.tv_usec = 20000;

        fd_set ready_sockets = listen_sockets;
        if (select(max_listen_socket_fd + 1, &ready_sockets, NULL, NULL, &timer) == 0) {
            continue;
        }
        for (int i = 0; i <= max_listen_socket_fd; i++) {
            if (FD_ISSET(i, &ready_sockets)) {
                // Get local IP
                sockaddr local_addr;
                socklen_t local_addr_size = sizeof(local_addr);
                getsockname(i, &local_addr, &local_addr_size);

                // Accept connection
                sockaddr addr;
                socklen_t addr_size = sizeof(addr);
                int client_sock = accept(i, &addr, &addr_size);
                if (client_sock == -1) {
                    std::cerr << "Error accepting connection from " << ip_to_string(addr)
                              << " on " << ip_to_string(local_addr) << std::endl;
                    continue;
                }

                // Process connection
                std::cout << "Accepting connection request from " << ip_to_string(addr)
                          << " on " << ip_to_string(local_addr) << std::endl;
                connections.push_back(std::async(std::launch::async, [&server] (const int fd) {
                        const size_t buffer_size = 256;
                        char buffer[buffer_size];
                        std::stringstream ss;

                        do {
                            ssize_t bytes_received = recv(fd, buffer, buffer_size, 0);
                            if (bytes_received < 0) {
                                throw std::runtime_error("Client disconnected unexpectedly.");
                            } else if (bytes_received == 0) {
                                break;
                            } else {
                                ss.write(buffer, bytes_received);
                            }
                            // Don't need to go past the header for the supported
                            // simple requests
                        } while (!std::strstr(buffer, "\r\n\r\n"));

                        HttpResponse response;
                        try {
                            HttpRequest request = HttpRequest::consume(ss.str());
                            response = server.processRequest(request);
                        } catch (const std::runtime_error &e) {
                            response = HttpResponse("400", "HTTP/1.0");
                        }
                        std::string response_str = response.encode();
                        send(fd, response_str.data(), response_str.size(), 0);

                        close(fd);
                    }, client_sock));
            }
        }

        // Check for threads that have finished execution
        for (auto it = connections.begin(); it != connections.end(); it++) {
            auto &f = *it;

            if (!f.valid()) {
                continue;
            }
            auto status = f.wait_for(std::chrono::seconds(0));
            if (status == std::future_status::ready) {
                try {
                    f.get(); // May throw an exception
                    it = connections.erase(it);
                } catch (const std::runtime_error &e) {
                    std::cerr << e.what() << std::endl;
                }
            }
        }
    }
}

std::vector<sockaddr> get_ip_address(const std::string &hostname,
                                     const unsigned short port) {
    addrinfo* query_result;
    addrinfo hints = {};
    hints.ai_socktype = SOCK_STREAM; // TCP
    std::vector<sockaddr> result;

    int status = getaddrinfo(hostname.c_str(),
                             std::to_string(port).c_str(),
                             &hints,
                             &query_result);
    if (status != 0) {
        throw std::runtime_error("Failed to get address for " + hostname
                + ", port " + std::to_string(port) + ": "
                + std::string(gai_strerror(status)));
    }

    for (addrinfo* a = query_result; a != NULL; a = a->ai_next) {
        result.push_back(*(struct sockaddr*)a->ai_addr);
    }

    freeaddrinfo(query_result);
    return result;
}

std::string ip_to_string(const sockaddr &address) {
    char str[INET6_ADDRSTRLEN] = {};
    switch (address.sa_family) {
        case AF_INET:
            inet_ntop(AF_INET, &((sockaddr_in*)(&address))->sin_addr, str, sizeof(str));
            break;
        case AF_INET6:
            inet_ntop(AF_INET6, &((sockaddr_in6*)(&address))->sin6_addr, str, sizeof(str));
            break;
        default:
            std::strcpy(str, "Unsupported Address Family");
    }
    return str;
}

void print_usage() {
    std::cerr << "Usage: web-server hostname port root" << std::endl;
}
