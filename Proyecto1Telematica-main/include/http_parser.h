// include/http_parser.h

#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include <string>
#include <vector> // Para cabeceras y cuerpo potencial
#include <map>    // Para cabeceras

class HttpRequest {
public:
    std::string method;
    std::string path;
    std::string version;
    std::map<std::string, std::string> headers; // Mapa para cabeceras
    std::string body;                         // String para el cuerpo

    // Modifica el método parse para que devuelva bool (éxito/fallo)
    // y tome el buffer completo como referencia para poder consumir datos.
    static bool parse(std::string& rawRequest, HttpRequest& req);
};

#endif