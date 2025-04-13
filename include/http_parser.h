//include/http_parser.h

#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include <string>

class HttpRequest {
public:
    std::string method;
    std::string path;
    std::string version;

    static HttpRequest parse(const std::string& rawRequest);
};

#endif
