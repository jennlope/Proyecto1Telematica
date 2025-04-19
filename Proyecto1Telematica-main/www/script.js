// www/script.js
console.log("Script.js cargado correctamente!");

// Cambia el texto de un párrafo en index.html
const mensajeElemento = document.getElementById('mensaje-script');
if (mensajeElemento) {
    mensajeElemento.textContent = "¡El archivo script.js se ejecutó correctamente!";
    mensajeElemento.style.color = "green";
    mensajeElemento.style.fontWeight = "bold";
} else {
    console.error("Elemento con ID 'mensaje-script' no encontrado.");
}

// Puedes añadir una alerta simple si quieres una confirmación visual más obvia
// alert("script.js cargado!");