//Main 

// src/server.cpp

#include <iostream>
#include <cstring>
#include <string>
#include <cstdlib>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

void startServer(int port) {
    int server_fd, client_fd;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

    // 1. Crear socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("❌ Error al crear el socket");
        exit(EXIT_FAILURE);
    }

    // 2. Permitir reutilización del puerto
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("❌ Error en setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 3. Asignar dirección y puerto
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;  // Escuchar en cualquier IP
    address.sin_port = htons(port);

    // 4. Asociar socket con dirección
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("❌ Error al hacer bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 5. Escuchar conexiones entrantes
    if (listen(server_fd, 10) < 0) {
        perror("❌ Error en listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    std::cout << "✅ Servidor escuchando en el puerto " << port << "..." << std::endl;

    // 6. Aceptar una conexión (solo para prueba)
    while (true) {
        if ((client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) {
            perror("❌ Error al aceptar conexión");
            continue;
        }

        std::cout << "📥 Cliente conectado desde: " << inet_ntoa(address.sin_addr) << ":" << ntohs(address.sin_port) << std::endl;

        // Enviar mensaje de prueba
        std::string mensaje = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 24\r\n\r\nHello from C++ server!\n";
        send(client_fd, mensaje.c_str(), mensaje.length(), 0);

        close(client_fd);  // Cerrar conexión
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

    startServer(port);

    return 0;
}
