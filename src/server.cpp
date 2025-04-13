// src/server.cpp

#include <iostream>
#include <cstring>
#include <string>
#include <cstdlib>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread> 

#define BUFFER_SIZE 1024


void handleClient(int client_fd, sockaddr_in client_address) {
    std::cout << "📥 Nueva conexión desde: " << inet_ntoa(client_address.sin_addr)
              << ":" << ntohs(client_address.sin_port) << std::endl;

    std::string mensaje =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 22\r\n"
        "Connection: close\r\n"
        "\r\n"
        "Hello from C++ server!\n";

    send(client_fd, mensaje.c_str(), mensaje.length(), 0);
    close(client_fd);
}

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
        int client_fd;
        sockaddr_in client_address;
        socklen_t addrlen = sizeof(client_address);
    
        client_fd = accept(server_fd, (struct sockaddr *)&client_address, &addrlen);
        if (client_fd < 0) {
            perror("❌ Error al aceptar conexión");
            continue;
        }
        //Crea un hilo para atender al cliente y lo suelta (detach)
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

    startServer(port);

    return 0;
}