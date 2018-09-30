#include "HttpRequest.h"

#include <sstream>
#include <string>
#include <vector>

std::string HttpRequest::getHost() const {
    for (const HttpHeader &header : headers) {
        if (header.name == "Host") {
            return header.value;
        }
    }
    return "";
}

void HttpRequest::setHost(const std::string &host) {
    for (HttpHeader &header : headers) {
        if (header.name == "Host") {
            header.value = host;
            return;
        }
    }
    addHeader("Host", host);
}

bool HttpRequest::hasHeader(const std::string &name) const {
    for (const HttpHeader &header : headers) {
        if (header.name == name) {
            return true;
        }
    }
    return false;
}

std::string HttpRequest::getHeader(const std::string &name) const {
    for (const HttpHeader &header : headers) {
        if (header.name == name) {
            return header.value;
        }
    }
    return "";
}

void HttpRequest::addHeader(const std::string &headerName, const std::string &headerValue) {
    this->addHeader(HttpHeader(headerName, headerValue));
}

void HttpRequest::addHeader(const HttpHeader &header) {
    for (HttpHeader &h : headers) {
        if (h.name == header.name) {
            h.value = header.value;
            return;
        }
    }
    this->headers.push_back(header);
}

std::string HttpRequest::encode() const {
    std::stringstream ss;
    ss << method << " " << path << " " << version << "\r\n";
    for (const HttpHeader &header : this->headers) {
        ss << header.toString() << "\r\n";
    }
    ss << "\r\n";
    return ss.str();
}

HttpRequest HttpRequest::consume(const std::string &wire) {
    HttpRequest result;

    bool found_empty_line = false;
    std::vector<std::string> lines;
    int previous_line_end = 0;
    for (size_t i = previous_line_end + 1; i < wire.size(); i++) {
        if (wire[i-1] == '\r' && wire[i] == '\n') {
            std::string s(wire.begin() + previous_line_end,
                          wire.begin() + i-1);
            if (s.empty()) {
                found_empty_line = true;
                break;
            }
            lines.push_back(s);
            previous_line_end = i + 1;
            i++;
        }
    }
    if (!found_empty_line || lines.size() < 2) {
        throw std::runtime_error("Malformed request");
    }

    std::vector<std::string> first_line_words;
    int previous_word_end = 0;
    for (size_t i = 0; i < lines[0].size(); i++) {
        if (lines[0][i] == ' '){
            std::string ss = lines[0].substr(previous_word_end, i-previous_word_end);
            first_line_words.push_back(ss);
            previous_word_end= i+1;
        }
    }
    first_line_words.push_back(lines[0].substr(previous_word_end,
                                               lines[0].size()-previous_word_end));
    if (first_line_words.size() != 3) {
        throw std::runtime_error("Malformed request");
    }

    result.setMethod(first_line_words[0]);
    result.setPath(first_line_words[1]);
    result.setHttpVersion(first_line_words[2]);

    for (size_t i = 1; i < lines.size(); i++) {
        result.addHeader(HttpHeader::fromString(lines[i]));
    }

    return result;
}
