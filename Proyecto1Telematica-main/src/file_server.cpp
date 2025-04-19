// src/file_server.cpp

#include "file_server.h"
#include <fstream>
#include <string> // Asegúrate de incluir string si usas C++17 o anterior para ends_with

// ... (otras funciones si las hubiera) ...

std::string FileServer::getMimeType(const std::string& filename) {
    // Obtener la extensión (si existe)
    size_t dot_pos = filename.find_last_of(".");
    if (dot_pos == std::string::npos) {
        return "application/octet-stream"; // Sin extensión
    }
    std::string ext = filename.substr(dot_pos); // Incluye el punto, ej: ".html"

    // Comparar extensiones (insensible a mayúsculas/minúsculas sería mejor, pero simple funciona)
    if (ext == ".html" || ext == ".htm") return "text/html";
    if (ext == ".css") return "text/css";
    if (ext == ".js") return "application/javascript";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif"; // Añadir más tipos comunes
    if (ext == ".txt") return "text/plain";
    if (ext == ".mp4") return "video/mp4"; // <--- AÑADIR ESTA LÍNEA
    if (ext == ".zip") return "application/zip"; // Para el archivo grande
    // Añadir más tipos MIME si es necesario (e.g., mp3, pdf, etc.)

    // Tipo por defecto si no se reconoce la extensión
    return "application/octet-stream";
}