// include/file_server.h

#ifndef FILE_SERVER_H
#define FILE_SERVER_H

#include <string>

class FileServer {
public:
    static bool readFile(const std::string& path, std::string& content);
    static std::string getMimeType(const std::string& filename);
};

#endif
