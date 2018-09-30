#pragma once

#include "HttpHeader.h"

#include <string>
#include <vector>

class HttpResponse {
 private:
    std::vector<HttpHeader> headers;
    std::string status_code;
    std::string http_version;
    std::string body;

 public:
    HttpResponse() : HttpResponse("500", "HTTP/1.0") {}
    HttpResponse(const std::string &status_code, const std::string &http_version)
        : status_code(status_code), http_version(http_version) {}

    std::string getStatusCode() { return this->status_code; }
    void setStatusCode(const std::string &status_code) { this->status_code = status_code; }

    std::string getVersion() { return this->http_version; }
    void setVersion(const std::string &version) { this->http_version = version; }

    std::string getBody() { return this->body; }
    void setBody(std::string body) { this->body = body; }

    bool hasHeader(const std::string &name) const;
    std::string getHeader(const std::string &name) const;
    void addHeader(const std::string &header_name, const std::string &header_value);
    void addHeader(const HttpHeader &header);

    std::string encode() const;
    static HttpResponse consume(std::string wire);
};

