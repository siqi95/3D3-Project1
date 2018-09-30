#pragma once

#include "HttpHeader.h"

#include <string>
#include <vector>

class HttpRequest{
 private:
    std::string method;
    std::string path;
    std::string version;
    std::vector<HttpHeader> headers;

 public:
    HttpRequest() : HttpRequest("GET", "/", "HTTP/1.0", "localhost") {}
    HttpRequest(const std::string& method,
            const std::string &path,
            const std::string &version,
            std::string host)
        : method(method), path(path), version(version) {
            addHeader("Host", host);
        }

    std::string getMethod() const { return this->method; }
    void setMethod(std::string methodName) { this->method = methodName; }

    std::string getPath() const { return this->path; }
    void setPath(std::string path) { this->path = path; }

    std::string getVersion() const { return this->version; }
    void setHttpVersion(const std::string &version) { this->version = version; }

    std::string getHost() const;
    void setHost(const std::string &host);

    bool hasHeader(const std::string &name) const;
    std::string getHeader(const std::string &name) const;
    void addHeader(const std::string &headerName, const std::string &headerValue);
    void addHeader(const HttpHeader &header);

    static HttpRequest consume(const std::string &wire);
    std::string encode() const;
};

