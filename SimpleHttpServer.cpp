#include "SimpleHttpServer.h"

#include <sys/stat.h>

#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>

bool dirExists(const std::string &path) {
    struct stat s;
    stat(path.c_str(), &s);
    return S_ISDIR(s.st_mode);
}

SimpleHttpServer::SimpleHttpServer(const std::string &hostname, short port, const std::string &root)
    : hostname(hostname), port(port), root(root) {
        if (this->root.back() != '/') {
            this->root += "/";
        }
        if (!dirExists(this->root)) {
            throw std::runtime_error("Specified root directory does not exist.");
        }
    }

HttpResponse SimpleHttpServer::processRequest(const HttpRequest &request) const {
    HttpResponse response;

    if (request.getHost() != this->hostname
            && request.getHost() != this->hostname + ":" + std::to_string(this->port)) {
        response.setStatusCode("400");
        response.setVersion("HTTP/1.0");
        response.addHeader("Connection", "close");
        return response;
    }
    if (request.getMethod() != "GET" && request.getMethod() != "HEAD") {
        response.setStatusCode("501");
        response.setVersion("HTTP/1.0");
        response.addHeader("Connection", "close");
        return response;
    }

    std::string filename = this->root + request.getPath();
    if (dirExists(filename)) {
        if (filename.back() != '/') {
            filename += "/";
        }
        filename += "index.html";
    }

    std::ifstream file(filename, std::ios::in | std::ios::binary);
    if (file.is_open()) {
        std::string file_data{std::istreambuf_iterator<char>(file),
                              std::istreambuf_iterator<char>()};
        response.setStatusCode("200");
        if (request.getMethod() != "HEAD") {
            response.setBody(file_data);
        }
        response.addHeader("Content-Length", std::to_string(response.getBody().size()));
        file.close();
    } else {
        response.setStatusCode("404");
    }
    response.setVersion("HTTP/1.0");
    response.addHeader("Connection", "close");

    return response;
}
