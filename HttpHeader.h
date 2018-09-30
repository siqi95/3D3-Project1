#pragma once

#include <string>

class HttpHeader {
 public:
    std::string name;
    std::string value;

    HttpHeader() {}
    HttpHeader(const std::string &name, const std::string &value) : name(name), value(value) {}

    std::string toString() const;
    static HttpHeader fromString(const std::string &string);
};
