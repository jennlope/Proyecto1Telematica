// src/file_server.cpp

#include "file_server.h"
#include <fstream>

bool FileServer::readFile(const std::string& path, std::string& content) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    content.assign((std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>());

    return true;
}

std::string FileServer::getMimeType(const std::string& filename) {
    if (filename.ends_with(".html") || filename.ends_with(".htm")) return "text/html";
    if (filename.ends_with(".css")) return "text/css";
    if (filename.ends_with(".js")) return "application/javascript";
    if (filename.ends_with(".png")) return "image/png";
    if (filename.ends_with(".jpg") || filename.ends_with(".jpeg")) return "image/jpeg";
    if (filename.ends_with(".txt")) return "text/plain";
    return "application/octet-stream";
}