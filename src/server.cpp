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
#include <ctime>
#include <mutex>

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

std::string logFilePath;
std::mutex logMutex;

void registrarLog(const std::string& ip, const std::string& metodo, const std::string& ruta, int codigo) {
    std::lock_guard<std::mutex> lock(logMutex);  // Para evitar conflicto entre hilos

    std::ofstream logFile(logFilePath, std::ios::app);
    if (!logFile.is_open()) return;

    // Obtener hora actual
    std::time_t now = std::time(nullptr);
    char tiempo[100];
    std::strftime(tiempo, sizeof(tiempo), "%Y-%m-%d %H:%M:%S", std::localtime(&now));

    logFile << "[" << tiempo << "] "
            << ip << " "
            << metodo << " " << ruta << " -> "
            << codigo << std::endl;

    logFile.close();
}


void handleClient(int client_fd, sockaddr_in client_address) {
    extern std::string documentRoot;

    std::cout << "📥 Nueva conexión desde: " << inet_ntoa(client_address.sin_addr)
              << ":" << ntohs(client_address.sin_port) << std::endl;

    try {
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
            std::string respuesta = generarRespuestaError(400, "Bad Request");
            registrarLog(inet_ntoa(client_address.sin_addr), "UNKNOWN", "INVALID", 400);
            send(client_fd, respuesta.c_str(), respuesta.length(), 0);
            close(client_fd);
            return;
        }

        HttpRequest request = HttpRequest::parse(requestStr);

        std::cout << "📨 Petición: " << request.method << " " << request.path << " " << request.version << std::endl;

        // Solo permitimos GET , HEAD Y POST, todo lo demás es 400
        if (request.method != "GET" && request.method != "HEAD" && request.method != "POST") {
            std::string respuesta = generarRespuestaError(400, "Bad Request");
            registrarLog(inet_ntoa(client_address.sin_addr), request.method, request.path, 400);
            send(client_fd, respuesta.c_str(), respuesta.length(), 0);
            close(client_fd);
            return;
        }

        if (request.path.empty() || request.path[0] != '/') {
            std::string respuesta = generarRespuestaError(400, "Bad Request");
            registrarLog(inet_ntoa(client_address.sin_addr), request.method, "INVALID_PATH", 400);
            send(client_fd, respuesta.c_str(), respuesta.length(), 0);
            close(client_fd);
            return;
        }

        std::string requestedPath = (request.path == "/") ? "/index.html" : request.path;

        std::string fullPath = documentRoot;
        if (fullPath.back() == '/' && requestedPath.front() == '/') {
            fullPath.pop_back();
        }
        fullPath += requestedPath;

        std::ifstream archivo(fullPath, std::ios::binary);
        if (!archivo.is_open()) {
            std::string respuesta = generarRespuestaError(404, "Not Found");
            registrarLog(inet_ntoa(client_address.sin_addr), request.method, request.path, 404);
            send(client_fd, respuesta.c_str(), respuesta.length(), 0);
            close(client_fd);
            return;
        }

        std::string contenido((std::istreambuf_iterator<char>(archivo)), std::istreambuf_iterator<char>());
        archivo.close();

        // Si el método es POST, extrae el cuerpo y lo muestra
        if (request.method == "POST") {
            // Encuentra inicio del cuerpo (después del doble salto de línea)
            size_t header_end = requestStr.find("\r\n\r\n");
            if (header_end != std::string::npos && header_end + 4 < requestStr.length()) {
                std::string cuerpo = requestStr.substr(header_end + 4);
                std::cout << "📦 Cuerpo recibido por POST:\n" << cuerpo << std::endl;
            }

            std::string mensaje =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 26\r\n"
                "Connection: close\r\n\r\n"
                "POST recibido correctamente.\n";

            send(client_fd, mensaje.c_str(), mensaje.length(), 0);
            registrarLog(inet_ntoa(client_address.sin_addr), request.method, request.path, 200);
            close(client_fd);
            return;
        }


        std::string encabezado =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: " + std::to_string(contenido.length()) + "\r\n"
            "Connection: close\r\n\r\n";

        std::string respuesta = encabezado;
        if (request.method == "GET") {
            respuesta += contenido;
        }
        registrarLog(inet_ntoa(client_address.sin_addr), request.method, request.path, 200);
        send(client_fd, respuesta.c_str(), respuesta.length(), 0);
        close(client_fd);

    } catch (...) {
        std::string respuesta = generarRespuestaError(400, "Bad Request");
        registrarLog(inet_ntoa(client_address.sin_addr), "EXCEPTION", "UNKNOWN", 400);
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

    logFilePath = logFile;
    documentRoot = rootFolder;

    startServer(port);
    return 0;
}
