#include "HttpHeader.h"

#include <algorithm>

inline void trim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [] (char c) {
        return !std::isspace(c);
    }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [] (char c) {
        return !std::isspace(c);
    }).base(), s.end());
}

HttpHeader HttpHeader::fromString(const std::string &string) {
    auto semicolon_pos = string.find(":");
    auto name = string.substr(0, semicolon_pos);
    auto value = string.substr(semicolon_pos+1);
    trim(name);
    trim(value);
    return HttpHeader(name, value);
}

std::string HttpHeader::toString() const {
    return this->name + ": " + this->value;
}
