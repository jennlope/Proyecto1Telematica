// src/http_parser.cpp

#include "http_parser.h"
#include <sstream>
#include <iostream> // Para debug si es necesario
#include <algorithm> // Para std::tolower

// Función helper para quitar espacios en blanco iniciales/finales
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (std::string::npos == first) {
        return str;
    }
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

// Función helper para convertir a minúsculas (para nombres de cabecera)
std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}


bool HttpRequest::parse(std::string& rawRequest, HttpRequest& req) {
    size_t header_end = rawRequest.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        // Si no encontramos el final de las cabeceras, la petición está incompleta o malformada
         // Podríamos necesitar leer más datos del socket en un caso real,
         // pero para este ejemplo, asumimos que recibimos todo o fallamos.
        return false;
    }

    std::string header_section = rawRequest.substr(0, header_end);
    req.body = rawRequest.substr(header_end + 4); // +4 para saltar \r\n\r\n

    std::istringstream header_stream(header_section);
    std::string line;

    // Leer la primera línea (línea de solicitud)
    if (std::getline(header_stream, line)) {
        std::istringstream linestream(line);
        if (!(linestream >> req.method >> req.path >> req.version)) {
            return false; // Error al parsear la línea de solicitud
        }
        // Quitar \r al final de la versión si existe
        if (!req.version.empty() && req.version.back() == '\r') {
            req.version.pop_back();
        }
    } else {
        return false; // No se pudo leer la primera línea
    }

    // Leer las cabeceras
    while (std::getline(header_stream, line) && !line.empty() && line != "\r") {
         // Quitar \r al final si existe
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) continue; // Saltar líneas vacías

        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string header_name = toLower(trim(line.substr(0, colon_pos)));
            std::string header_value = trim(line.substr(colon_pos + 1));
            req.headers[header_name] = header_value;
        } else {
             // Cabecera mal formada? Podríamos ignorarla o retornar false
             // return false;
        }
    }

     // Validar Content-Length si es POST y asegurar que el cuerpo leído coincide
     if (req.method == "POST") {
         auto it = req.headers.find("content-length");
         if (it != req.headers.end()) {
             try {
                 size_t expected_length = std::stoul(it->second);
                 // Aquí podríamos necesitar leer más si req.body.length() < expected_length
                 // Por simplicidad, asumimos que el recv inicial fue suficiente o fallamos.
                 // En un servidor robusto, tendrías que leer exactamente expected_length bytes.
                 if (req.body.length() > expected_length) {
                     req.body = req.body.substr(0, expected_length); // Truncar si leímos de más
                 } else if (req.body.length() < expected_length) {
                     // Necesitaríamos leer más del socket. Retornamos false por simplicidad aquí.
                     std::cerr << "Advertencia: Cuerpo POST incompleto recibido." << std::endl;
                     return false; // O manejar lectura adicional
                 }
             } catch (const std::exception& e) {
                 std::cerr << "Error al parsear Content-Length: " << e.what() << std::endl;
                 return false; // Content-Length inválido
             }
         } else {
             // POST sin Content-Length es inválido según HTTP/1.1
             // A menos que sea Transfer-Encoding: chunked (no soportado aquí)
             std::cerr << "Error: POST sin Content-Length." << std::endl;
             return false; // 411 Length Required debería ser enviado por el handler
         }
     }


    return true;
}