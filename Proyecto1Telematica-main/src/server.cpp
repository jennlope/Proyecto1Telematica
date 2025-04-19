// src/server.cpp

// --- Includes ---
// Clases del proyecto
#include "http_parser.h"
#include "file_server.h"

// Standard C++ Library
#include <iostream>     // Para std::cout, std::cerr
#include <fstream>      // Para std::ofstream, std::ifstream
#include <sstream>      // Para std::ostringstream
#include <string>       // Para std::string, std::to_string
#include <vector>       // Usado indirectamente por headers, buena práctica incluirlo
#include <map>          // Usado en http_parser.h
#include <thread>       // Para std::thread
#include <mutex>        // Para std::mutex, std::lock_guard
#include <chrono>       // Para std::chrono (timestamps)
#include <iomanip>      // Para std::put_time (formato de timestamp)
#include <filesystem>   // Para std::filesystem (chequear DocumentRoot)
#include <stdexcept>    // Para std::exception y derivadas
#include <cstdlib>      // Para EXIT_FAILURE (aunque no se usa explícitamente ahora)

// POSIX / Sockets specific
#include <unistd.h>     // Para close()
#include <sys/socket.h> // Para API de sockets (socket, bind, listen, accept, etc.)
#include <netinet/in.h> // Para struct sockaddr_in, INADDR_ANY, htons()
#include <arpa/inet.h>  // Para inet_ntoa(), ntohs()
#include <cstring>      // Para strerror()
#include <cerrno>       // Para errno

// --- Defines ---
#define BUFFER_SIZE 8192 // Aumentado el buffer

// --- Globales ---
std::string documentRoot;    // Ruta base para los recursos
std::ofstream log_stream;    // Stream para el archivo de log
std::mutex log_mutex;      // Mutex para proteger el acceso al log
std::string logFilePath;     // Ruta al archivo de log

// --- Función de Logging Mejorada ---
void logMessage(const std::string& message) {
    // Obtener timestamp
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::ostringstream timestamp_stream;
    timestamp_stream << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S");
    std::string timestamp = timestamp_stream.str();

    std::lock_guard<std::mutex> guard(log_mutex); // Bloqueo para concurrencia
    // Escribir en consola
    std::cout << "[" << timestamp << "] " << message << std::endl;
    // Escribir en archivo si está abierto
    if (log_stream.is_open()) {
        log_stream << "[" << timestamp << "] " << message << std::endl;
        // log_stream.flush(); // Opcional: asegura escritura inmediata, puede afectar rendimiento
    }
}

// --- Generador de Respuestas de Error ---
std::string generarRespuestaError(int codigo, const std::string& mensaje) {
    std::string cuerpo = "<html><head><title>" + std::to_string(codigo) + " " + mensaje + "</title></head>"
                         "<body><h1>" + std::to_string(codigo) + " " + mensaje + "</h1></body></html>";
    std::string encabezado =
        "HTTP/1.1 " + std::to_string(codigo) + " " + mensaje + "\r\n"
        "Content-Type: text/html\r\n" // Errores siempre son HTML
        "Content-Length: " + std::to_string(cuerpo.length()) + "\r\n"
        "Connection: close\r\n\r\n";

    std::string response_line = "HTTP/1.1 " + std::to_string(codigo) + " " + mensaje;
    // Loguear antes de retornar para asegurar que se loguea aunque send() falle
    logMessage("📬 Respuesta enviada: " + response_line);

    return encabezado + cuerpo;
}

// --- Manejador de Conexiones Cliente ---
void handleClient(int client_fd, sockaddr_in client_address) {
    std::string client_ip = inet_ntoa(client_address.sin_addr);
    int client_port = ntohs(client_address.sin_port);
    std::string client_id = client_ip + ":" + std::to_string(client_port); // Identificador para logs

    try {
        logMessage("📥 Nueva conexión desde: " + client_id);

        char buffer[BUFFER_SIZE] = {0};
        // Nota: recv puede no leer toda la petición HTTP de una vez.
        // Un servidor robusto leería en bucle hasta encontrar "\r\n\r\n"
        // o hasta llenar el buffer/timeout.
        int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

        if (bytes_received < 0) {
            logMessage("❌ Error recibiendo datos (recv): " + std::string(strerror(errno)) + " (" + client_id + ")");
            close(client_fd);
            return;
        }
         if (bytes_received == 0) {
            logMessage("🔌 Conexión cerrada por el cliente antes de enviar datos (" + client_id + ")");
            close(client_fd);
            return;
        }
        buffer[bytes_received] = '\0'; // Asegurar null terminator
        std::string requestStr(buffer);

        HttpRequest request;
        // Usar el nuevo parser
        if (!HttpRequest::parse(requestStr, request)) {
            logMessage("❌ Petición malformada o incompleta desde " + client_id);
            // No se puede generar respuesta si el parseo falló muy temprano
            // Intentamos enviar Bad Request igualmente
            std::string respuesta = generarRespuestaError(400, "Bad Request");
            send(client_fd, respuesta.c_str(), respuesta.length(), 0); // Ignorar errores de send aquí
            close(client_fd);
            return;
        }

        logMessage("📨 Petición recibida (" + client_id + "): " + request.method + " " + request.path + " " + request.version);

        // --- Manejo de Métodos ---
        std::string respuesta;
        if (request.method == "GET" || request.method == "HEAD") {
            // Validaciones de seguridad básicas para el path
            if (request.path.empty() || request.path[0] != '/' || request.path.find("..") != std::string::npos) {
                 logMessage("❌ Petición inválida (Bad Request path): " + request.path + " (" + client_id + ")");
                 respuesta = generarRespuestaError(400, "Bad Request");
            } else {
                 std::string requestedPath = request.path;
                 if (requestedPath == "/") requestedPath = "/index.html"; // Archivo por defecto

                 // Construir ruta completa (considerar sanitización/normalización más robusta)
                 std::string fullPath = documentRoot + requestedPath;

                 std::ifstream archivo(fullPath, std::ios::binary);
                 if (!archivo.good()) { // No existe o no accesible inicialmente
                     logMessage("❌ Recurso no encontrado (404): " + fullPath + " (" + client_id + ")");
                     respuesta = generarRespuestaError(404, "Not Found");
                 } else if (!archivo.is_open()) { // Existe pero no se pudo abrir (permisos?)
                     logMessage("❌ Acceso prohibido (403): " + fullPath + " (" + client_id + ")");
                     respuesta = generarRespuestaError(403, "Forbidden");
                 } else {
                     // --- Manejo de Content-Type Dinámico ---
                     std::string mimeType = FileServer::getMimeType(fullPath);
                     std::string contenido((std::istreambuf_iterator<char>(archivo)), std::istreambuf_iterator<char>());
                     archivo.close();

                     std::string response_line = "HTTP/1.1 200 OK";
                     std::string encabezado =
                         response_line + "\r\n"
                         "Content-Type: " + mimeType + "\r\n"
                         "Content-Length: " + std::to_string(contenido.length()) + "\r\n"
                         "Connection: close\r\n\r\n";

                     respuesta = encabezado;
                     if (request.method == "GET") {
                         respuesta += contenido;
                     }
                     // Loguear antes de enviar
                     logMessage("📬 Respuesta enviada: " + response_line + " (" + mimeType + ", " + std::to_string(contenido.length()) + " bytes) a " + client_id);
                 }
            }
        } else if (request.method == "POST") {
            // --- Manejo Básico de POST ---
            // Loguear info del POST
             logMessage("   POST Info ("+ client_id +"): Target=" + request.path +", Content-Length=" + request.headers["content-length"] + ", Body size=" + std::to_string(request.body.length()));
             // Loguear cuerpo si es pequeño (cuidado con logs muy grandes)
             if (request.body.length() < 256) { // Limitar log del cuerpo
                  logMessage("   POST Body ("+ client_id +"): " + request.body);
             } else {
                  logMessage("   POST Body ("+ client_id +"): [Truncado, >256 bytes]");
             }

            // Respuesta simple de éxito para POST
            std::string postResponseBody = "<html><body><h1>POST request received successfully!</h1>"
                                         "<p>Path: " + request.path + "</p>"
                                         "<p>Received " + std::to_string(request.body.length()) + " bytes.</p>"
                                         "</body></html>";
            std::string response_line = "HTTP/1.1 200 OK";
            respuesta = response_line + "\r\n"
                        "Content-Type: text/html\r\n"
                        "Content-Length: " + std::to_string(postResponseBody.length()) + "\r\n"
                        "Connection: close\r\n\r\n" + postResponseBody;
            logMessage("📬 Respuesta enviada: " + response_line + " (POST recibido) a " + client_id);

        } else {
            // Método no soportado (PUT, DELETE, etc.) o inválido
            logMessage("❌ Método no permitido (405): " + request.method + " (" + client_id + ")");
            respuesta = generarRespuestaError(405, "Method Not Allowed");
        }

        // Enviar la respuesta construida
        int bytes_sent = send(client_fd, respuesta.c_str(), respuesta.length(), 0);
        if (bytes_sent < 0) {
             logMessage("❌ Error enviando respuesta (send): " + std::string(strerror(errno)) + " (" + client_id + ")");
        } else if (bytes_sent < (int)respuesta.length()) {
             logMessage("⚠️ Advertencia: Respuesta enviada parcialmente (" + std::to_string(bytes_sent) + "/" + std::to_string(respuesta.length()) + " bytes) a " + client_id);
        }

    } catch (const std::exception& e) {
        logMessage("❌ Excepción interna en handleClient(" + client_id + "): " + std::string(e.what()));
        // Intentar enviar error 500 si es posible
        try {
             std::string respuesta = generarRespuestaError(500, "Internal Server Error");
             send(client_fd, respuesta.c_str(), respuesta.length(), 0); // Ignorar error de send aquí
        } catch(...) { /* No hacer nada si send falla aquí */ }
    } catch (...) {
        logMessage("❌ Excepción desconocida atrapada en handleClient(" + client_id + ").");
         try {
             std::string respuesta = generarRespuestaError(500, "Internal Server Error");
             send(client_fd, respuesta.c_str(), respuesta.length(), 0); // Ignorar error de send aquí
         } catch(...) { /* No hacer nada si send falla aquí */ }
    }

    // Siempre cerrar el socket al final del manejo del cliente
    close(client_fd);
    logMessage("🔌 Conexión cerrada con " + client_id);
}

// --- Función Principal del Servidor ---
void startServer(int port) {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;

    // Crear socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        // Usar cerr para errores críticos antes de que el log esté listo
        std::cerr << "❌ Error crítico al crear el socket: " << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }

    // Configurar opciones del socket (reusar dirección/puerto)
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        std::cerr << "❌ Error crítico en setsockopt: " << strerror(errno) << std::endl;
        close(server_fd); // Liberar el socket creado
        exit(EXIT_FAILURE);
    }

    // Configurar dirección del servidor
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Escuchar en todas las interfaces
    address.sin_port = htons(port);       // Puerto en network byte order

    // Enlazar socket a la dirección y puerto
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        std::cerr << "❌ Error crítico al hacer bind al puerto " << port << ": " << strerror(errno) << std::endl;
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Poner el socket en modo escucha
    if (listen(server_fd, 10) < 0) { // 10 es el backlog (cola de conexiones pendientes)
        std::cerr << "❌ Error crítico en listen: " << strerror(errno) << std::endl;
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    logMessage("✅ Servidor escuchando en el puerto " + std::to_string(port) + "...");

    // Bucle principal para aceptar conexiones
    while (true) {
        struct sockaddr_in client_address;
        socklen_t addrlen = sizeof(client_address);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_address, &addrlen);

        if (client_fd < 0) {
            // Este error puede ocurrir si el servidor se interrumpe, no siempre es crítico
            logMessage("⚠️ Error al aceptar conexión (accept): " + std::string(strerror(errno)) + ". Continuando...");
            // Podríamos añadir un pequeño sleep aquí si el error persiste mucho
            continue;
        }

        // Crear y lanzar hilo para manejar al cliente (desacoplado)
        try {
             std::thread(handleClient, client_fd, client_address).detach();
        } catch (const std::system_error& e) {
             logMessage("❌ Error crítico al crear hilo: " + std::string(e.what()) + ". Cerrando conexión.");
             close(client_fd);
             // Considerar si este error debe detener el servidor
        }
    }

    // Código de limpieza (normalmente no se alcanza en este diseño simple)
    close(server_fd);
    logMessage("🛑 Servidor detenido.");
    if (log_stream.is_open()) {
        log_stream.close();
    }
}

// --- Punto de Entrada Principal ---
int main(int argc, char* argv[]) {
    // Validar argumentos de línea de comandos
    if (argc != 4) {
        std::cerr << "Uso: " << argv[0] << " <PUERTO> <LOG_FILE> <DOCUMENT_ROOT>\n";
        return 1;
    }

    // Parsear Puerto
    int port;
    try {
         port = std::stoi(argv[1]);
         if (port <= 0 || port > 65535) { // Validar rango de puerto
             throw std::out_of_range("Puerto fuera de rango (1-65535)");
         }
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: Puerto inválido '" << argv[1] << "'. " << e.what() << std::endl;
        return 1;
    }

    // Obtener rutas de archivo de log y document root
    logFilePath = argv[2];
    documentRoot = argv[3];

    // --- Configurar Logger ---
    log_stream.open(logFilePath, std::ios::app); // Modo append
    if (!log_stream.is_open()) {
         // Usar cerr porque el log a archivo falló
         std::cerr << "⚠️ Advertencia: No se pudo abrir el archivo de log '" << logFilePath << "'. Logueando solo a consola." << std::endl;
         // No es fatal, continuamos sin log a archivo
    } else {
        // Loguear el inicio usando la función ahora que (potencialmente) está lista
        logMessage("📝 Log iniciado. Escribiendo a consola y a " + logFilePath);
    }

    // Verificar DocumentRoot
    try {
         if (!std::filesystem::exists(documentRoot)) {
              throw std::runtime_error("La ruta especificada no existe.");
         }
         if (!std::filesystem::is_directory(documentRoot)) {
             throw std::runtime_error("La ruta especificada no es un directorio.");
         }
         // Loguear éxito
         logMessage("📂 DocumentRoot verificado y establecido en: " + documentRoot);
    } catch (const std::exception& e) {
         logMessage("❌ Error crítico con DocumentRoot '" + documentRoot + "': " + e.what());
          if (log_stream.is_open()) log_stream.close(); // Cerrar log si se abrió
         return 1; // Salir si DocumentRoot es inválido
    }

    // Iniciar el servidor
    startServer(port); // Esta función contiene el bucle infinito

    // Código inalcanzable en este diseño simple
    if (log_stream.is_open()) {
        log_stream.close();
    }
    return 0; // Opcional, ya que startServer no retorna normalmente
}