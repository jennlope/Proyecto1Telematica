## PROYECTO HTTP SERVER
Telemática





### DESARROLLADO POR:
#### Santiago Alexander Cardenas Laverde
#### Jennifer Andrea López Gómez
#### James Andrei Valencia

---

## **1. Introducción**

Este proyecto consiste en la implementación de un servidor web desarrollado en C++ que cumple con el protocolo HTTP/1.1, y este puede atender peticiones `GET`, `HEAD` y `POST`, asi como manejar múltiples clientes concurrentemente mediante hilos, registrar logs de actividad, y servir archivos estáticos como HTML, imágenes, CSS, JS y archivos de texto. El servidor fue desplegado en una instancia EC2 de AWS y es accesible mediante DNS dinámico a través de No-IP.

---

## **2. Desarrollo e Implementación**

### 2.1 Estructura del Proyecto

```
Proyecto1Telematica/
├── include/
│   ├── file_server.h
│   └── http_parser.h
├── src/
│   ├── file_server.cpp
│   ├── http_parser.cpp
│   └── server.cpp
├── www/
│   ├── index.html
│   ├── prueba1/
│   ├── prueba2/
│   ├── prueba3/
│   └── prueba4/
├── logs/
│   └── log.txt
├── makefile
├── server
└── telematica.pem
```

---

### 2.2 Explicación del código principal (`server.cpp`)

#### `main(int argc, char* argv[])`
- Verifica que se proporcionen 3 argumentos: puerto, archivo de log y carpeta raíz (`documentRoot`).
- Convierte el puerto a `int`, guarda el `documentRoot`, y llama a `startServer`.

---

#### `startServer(int port)`
- Crea un socket con `socket(AF_INET, SOCK_STREAM, 0)`.
- Usa `setsockopt` para reutilizar el puerto (evita error en reinicios).
- Asocia el socket a una dirección IP y puerto con `bind`.
- Inicia la escucha de conexiones con `listen`.
- Entra en un bucle infinito donde:
  - Llama a `accept()` para aceptar nuevas conexiones.
  - Por cada cliente, lanza un hilo con `std::thread(handleClient, ...)`.

---

#### `handleClient(int client_fd, sockaddr_in client_address)`
- Muestra la IP del cliente.
- Lee el mensaje enviado con `recv`.
- Verifica si el mensaje contiene `HTTP/` (para validar formato).
- Parsea la solicitud usando `HttpRequest::parse`.
- Si el método no es `GET` o `HEAD`, responde con `400 Bad Request`.
- Verifica y ajusta la ruta solicitada. Si es `/`, responde con `/index.html`.
- Construye la ruta completa del archivo solicitado.
- Si no se puede abrir, responde con `404 Not Found`.
- Si se abre correctamente:
  - Lee su contenido.
  - Arma un encabezado con `200 OK` y `Content-Length`.
  - Si es `GET`, agrega el contenido del archivo.
- Envía la respuesta al cliente y cierra el socket.

---

#### `generarRespuestaError(int codigo, const std::string& mensaje)`
- Devuelve una respuesta HTTP con el código de estado especificado.
- Incluye un cuerpo HTML básico con el mensaje del error.

---

### 2.3 Módulos auxiliares

#### `http_parser.h / http_parser.cpp`

Clase `HttpRequest`:
- Propiedades: `method`, `path`, `version`.
- Método `static parse()`: Extrae los valores desde la primera línea de la solicitud HTTP (por ejemplo: `GET /index.html HTTP/1.1`).

---

#### `file_server.h / file_server.cpp`

- Estas funciones pueden incluir herramientas para servir archivos de forma modular, aunque parte del manejo actual se realiza directamente en `server.cpp`.

---

### 2.4 Archivos servidos (`www/`)

Contiene archivos accesibles por navegador:

- `index.html`: Página principal con enlaces a todas las pruebas.
- `prueba1/`: Muestra una imagen.
- `prueba2/`: Reproduce un video.
- `prueba3/`: Página con estilos CSS, JS, e imagen.
- `prueba4/`: Archivo de texto grande para pruebas de carga.

---

## **3. Conclusiones**

- Se implementó un servidor funcional y concurrente desde cero usando C++ y sockets BSD.
- El código es modular, con separación entre parsing HTTP y lógica del servidor.
- Se respondieron correctamente solicitudes HTTP con códigos 200, 400 y 404.
- Se comprobó su funcionamiento sirviendo archivos estáticos desde `www/`.
- Se logró el despliegue en la nube (AWS EC2) y su vinculación con un subdominio gratuito usando No-IP.
- Quedan como futuras mejoras: soporte de métodos POST completos, manejo de MIME types, y seguridad.

---

## **4. Referencias**

- RFC 2616: [Hypertext Transfer Protocol -- HTTP/1.1](https://datatracker.ietf.org/doc/html/rfc2616)
- Beej’s Guide to Network Programming: https://beej.us/guide/bgnet/
- Documentación oficial de C++: https://en.cppreference.com/
- No-IP Dynamic DNS: https://www.noip.com/
- AWS EC2 Documentation: https://docs.aws.amazon.com/ec2/
