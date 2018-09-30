#pragma once

#include "HttpRequest.h"
#include "HttpResponse.h"

#include <memory>

class SimpleHttpServer {
 private:
     std::string hostname;
     short port;
     std::string root;

 public:
    SimpleHttpServer(const std::string &hostname, short port, const std::string &root);

    HttpResponse processRequest(const HttpRequest &request) const;
};
