// src/http_parser.cpp

#include "http_parser.h"
#include <sstream>

HttpRequest HttpRequest::parse(const std::string& rawRequest) {
    std::istringstream stream(rawRequest);
    HttpRequest req;
    std::string line;

    // Leer la primera línea
    if (std::getline(stream, line)) {
        std::istringstream linestream(line);
        linestream >> req.method >> req.path >> req.version;
    }

    return req;
}