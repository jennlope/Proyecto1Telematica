#include "http_parser.h"
#include "file_server.h"
#include <filesystem>
#include <iostream>
#include <cstring>
#include <string>
#include <cstdlib>
#include <thread>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fstream>

#define BUFFER_SIZE 1024

std::string documentRoot; // Ruta base para los recursos

// Fase 6 - Errores
std::string generarRespuestaError(int codigo, const std::string& mensaje) {
    std::string cuerpo = "<h1>" + std::to_string(codigo) + " " + mensaje + "</h1>";
    std::string encabezado =
        "HTTP/1.1 " + std::to_string(codigo) + " " + mensaje + "\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: " + std::to_string(cuerpo.length()) + "\r\n"
        "Connection: close\r\n\r\n";
    return encabezado + cuerpo;
}

//Modificacion por error 500, antes se caia de una vez el servidor
void handleClient(int client_fd, sockaddr_in client_address) {
    try {
        extern std::string documentRoot;

        std::cout << "📥 Nueva conexión desde: " << inet_ntoa(client_address.sin_addr)
                  << ":" << ntohs(client_address.sin_port) << std::endl;

        char buffer[BUFFER_SIZE] = {0};
        int bytes_received = recv(client_fd, buffer, sizeof(buffer), 0);

        if (bytes_received <= 0) {
            std::cerr << "❌ Error o conexión cerrada por el cliente." << std::endl;
            close(client_fd);
            return;
        }

        std::string requestStr(buffer);

        // Validación básica de petición
        if (requestStr.find("HTTP/") == std::string::npos) {
            std::cerr << "❌ Petición malformada o vacía." << std::endl;
            std::string respuesta = generarRespuestaError(400, "Bad Request");
            send(client_fd, respuesta.c_str(), respuesta.length(), 0);
            close(client_fd);
            return;
        }

        HttpRequest request = HttpRequest::parse(requestStr);

        std::cout << "📨 Petición: " << request.method << " " << request.path << " " << request.version << std::endl;

        if (request.method != "GET" && request.method != "HEAD" && request.method != "POST") {
            std::string respuesta = generarRespuestaError(405, "Method Not Allowed");
            send(client_fd, respuesta.c_str(), respuesta.length(), 0);
            close(client_fd);
            return;
        }

        if (request.path.empty() || request.path[0] != '/') {
            std::string respuesta = generarRespuestaError(400, "Bad Request");
            send(client_fd, respuesta.c_str(), respuesta.length(), 0);
            close(client_fd);
            return;
        }

        std::string requestedPath = request.path;
        if (requestedPath == "/") requestedPath = "/index.html";
        std::string fullPath = documentRoot + requestedPath;

        std::ifstream archivo(fullPath, std::ios::binary);
        if (!archivo.good()) {
            std::string respuesta = generarRespuestaError(404, "Not Found");
            send(client_fd, respuesta.c_str(), respuesta.length(), 0);
            close(client_fd);
            return;
        }

        if (!archivo.is_open()) {
            std::string respuesta = generarRespuestaError(403, "Forbidden");
            send(client_fd, respuesta.c_str(), respuesta.length(), 0);
            close(client_fd);
            return;
        }

        std::string contenido((std::istreambuf_iterator<char>(archivo)), std::istreambuf_iterator<char>());
        archivo.close();

        std::string encabezado =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: " + std::to_string(contenido.length()) + "\r\n"
            "Connection: close\r\n\r\n";
        std::string respuesta = encabezado;

        if (request.method == "GET") {
            respuesta += contenido;
        }

        send(client_fd, respuesta.c_str(), respuesta.length(), 0);
        close(client_fd);

    } catch (const std::exception& e) {
        std::cerr << "❌ Excepción interna: " << e.what() << std::endl;
        std::string respuesta = generarRespuestaError(500, "Internal Server Error");
        send(client_fd, respuesta.c_str(), respuesta.length(), 0);
        close(client_fd);
    } catch (...) {
        std::cerr << "❌ Excepción desconocida atrapada.\n";
        std::string respuesta = generarRespuestaError(500, "Internal Server Error");
        send(client_fd, respuesta.c_str(), respuesta.length(), 0);
        close(client_fd);
    }
}


void startServer(int port) {
    int server_fd, client_fd;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("❌ Error al crear el socket");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("❌ Error en setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("❌ Error al hacer bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("❌ Error en listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    std::cout << "✅ Servidor escuchando en el puerto " << port << "..." << std::endl;

    while (true) {
        sockaddr_in client_address;
        socklen_t addrlen = sizeof(client_address);
        client_fd = accept(server_fd, (struct sockaddr *)&client_address, &addrlen);
        if (client_fd < 0) {
            perror("❌ Error al aceptar conexión");
            continue;
        }

        std::thread(handleClient, client_fd, client_address).detach();
    }

    close(server_fd);
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Uso: " << argv[0] << " <PUERTO> <LOG_FILE> <DOCUMENT_ROOT>\n";
        return 1;
    }

    int port = std::stoi(argv[1]);
    std::string logFile = argv[2];
    std::string rootFolder = argv[3];

    documentRoot = rootFolder;

    startServer(port);
    return 0;
}
