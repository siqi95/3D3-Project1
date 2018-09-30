#include "HttpRequest.h"
#include "HttpResponse.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <regex>
#include <string>
#include <sstream>

void download_file(const std::string &url) {
    const static std::regex ulr_pattern(
                std::string("^(?:http:\\/\\/)?(\\[[a-f0-9:]+|[a-z0-9-._~%]+)")
                + "(?:\\:(\\d{1,5}))?(?:(\\/[\\/a-z0-9-._~%]*(?:\\?[\\/a-z0-9-=._~%]*)?)"
                + "(?:\\#[\\/a-z0-9--._~%]*)?)?$",
            std::regex_constants::ECMAScript | std::regex_constants::icase);

    // Parse url
    std::smatch url_match;
    std::regex_match(url, url_match, ulr_pattern);
    if (url_match.size() < 2) {
        std::cerr << "Invalid url " << url << std::endl;
        return;
    }
    std::string host = url_match[1];
    unsigned short port;
    if (url_match.size() >= 2 && url_match[2].length() > 0) {
        try {
            port = std::stoi(url_match[2]);
            if (port > std::numeric_limits<unsigned short>::max()) {
                throw std::out_of_range("");
            }
        } catch (const std::logic_error&) {
            std::cerr << "port must be an integer between 0 and 65535, found "
                << url_match[2] << std::endl;
            return;
        }
    } else {
        port = 80;
    }
    std::string path;
    if (url_match.size() >= 3 && url_match[3].length() > 0) {
        path = url_match[3];
    } else {
        path = "/";
    }

    // Get server address
    addrinfo *address;
    addrinfo hints = {};
    hints.ai_socktype = SOCK_STREAM;
    int status = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &address);
    if (status != 0) {
        std::cerr << "Error getting server address for " << host << ": "
            << gai_strerror(status) << std::endl;
        return;
    }

    // Connect to server
    int sock = socket(address->ai_family, SOCK_STREAM, 0);
    if (connect(sock, address->ai_addr, address->ai_addrlen) == -1) {
        std::cerr << "Error connecting to host " << host << std::endl;
        return;
    }

    // Send request
    HttpRequest request("GET", path, "HTTP/1.0", host);
    request.addHeader("Connection", "close");
    std::string request_str = request.encode();
    send(sock, request_str.c_str(), request_str.size(), 0);
    std::cout << "Sent request for " << path << " to "  << host
        << " on port " << port << std::endl;

    // Receive response
    std::stringstream ss;
    const size_t buffer_size = 256;
    char buffer[buffer_size];
    size_t header_length = std::string::npos;
    size_t body_length = std::string::npos;
    size_t total_bytes_received = 0;
    do {
        ssize_t bytes_received = recv(sock, buffer, buffer_size, 0);
        if (bytes_received < 0) {
            std::cerr << "Connection error" << std::endl;
            return;
        } else if (bytes_received == 0) { // Connection closed
            if (body_length != std::string::npos
                    && total_bytes_received < header_length + body_length) {
                std::cerr << "Server " << host <<
                    " closed connection before full message was recieved.";
            }
            break;
        } else {
            total_bytes_received += bytes_received;
            ss.write(buffer, bytes_received);
        }

        // See if the Content-Length can be extracted from the header
        if (header_length == std::string::npos) { // If we haven't done so already
            std::string partial_message = ss.str();
            // Only if we have received the full header
            if ((header_length = partial_message.find("\r\n\r\n")) != std::string::npos) {
                header_length += 4; // Account for the \r\n\r\n

                HttpResponse partial_response;
                try {
                    partial_response = HttpResponse::consume(partial_message);
                } catch (const std::runtime_error &e) {
                    std::cerr << "Error: " << e.what() << std::endl;
                }

                if (partial_response.hasHeader("Content-Length")) {
                    try {
                        body_length = std::stoll(partial_response.getHeader("Content-Length"));
                    } catch (const std::logic_error &e) {
                        std::cerr << "Malformed response from " << host
                            << ", error parsing Content-Length value: "
                            << e.what() << std::endl;
                        body_length = std::string::npos; // Continue without Content-Length
                    }
                }
            }
        }

        // Do not wait for the server to close the connection if the entire message has been
        // recieved
        if (body_length != std::string::npos
                && header_length + body_length <= total_bytes_received) {
            break;
        }
    } while (true);

    close(sock);

    HttpResponse response;
    try {
        response = HttpResponse::consume(ss.str());
    } catch (const std::runtime_error &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return;
    }
    if (response.getStatusCode() == "200") {
        std::cerr << "Request successful. Status code: 200" << std::endl;
    } else {
        std::cerr << "Request unsuccessful. Status code: " << response.getStatusCode()
            << std::endl;
        return;
    }

    // Extract filename using regular expressions
    const static std::regex path_pattern(std::string("^(?:[\\/a-z0-9-._~%])*?([a-z0-9-._~%]*)")
            + "(?:\\?[\\/a-z0-9-.=_~%]*)?$",
            std::regex_constants::ECMAScript | std::regex_constants::icase);
    std::smatch path_match;
    std::regex_match(path, path_match, path_pattern);
    std::string filename = "";
    if (path_match.size() == 2) {
        filename = path_match[1];
    }
    if (filename == "") { // if failed matching, or filename empty, use default
        filename = "index.html";
    }

    std::ofstream file("./" + filename, std::ios::out | std::ios::trunc);
    if (!file.good()) {
        std::cerr << "Error opening file for writing: " << filename << std::endl;
    }
    file << response.getBody();
    file.flush();
    file.close();
    std::cerr << "Success downloading file " << filename << std::endl;
}

int main(int argc, char **argv) {
    for (int address_index = 1; address_index < argc; address_index++) {
        download_file(argv[address_index]);

        if (address_index != argc - 1) { // Insert a newline between requests
            std::cerr << std::endl;
        }
    }
}
