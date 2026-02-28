# Cerradura Electrónica - Taller Proyecto I

## 📖 Descripción del Proyecto
Este repositorio contiene el código fuente completo para el prototipo de una **Cerradura Electrónica Inteligente**, desarrollado como parte de la asignatura **Taller de Proyecto I**.

El proyecto integra un sistema embebido programado en C con una arquitectura de servidor backend, permitiendo el control de acceso, gestión de usuarios y monitoreo remoto del estado de la cerradura.

## 📂 Estructura del Repositorio

El proyecto se divide en dos módulos principales:

* **`/firmware`**: Código fuente del sistema embebido escrito en **C**.
    * Control de periféricos y actuadores.
    * Lógica de validación de acceso local.
    * Comunicación con el servidor.
* **`/backend`**: Servidor y lógica del lado del servidor.
    * API para la comunicación con el dispositivo.
    * Base de datos de usuarios y registros (logs).
    * Interfaz de gestión (si aplica).

## 🛠️ Tecnologías Utilizadas

### Firmware / Hardware
* **Lenguaje:** C
* **Microcontrolador:** LPC 4337 (EDU-CIAA), ESP32
* **Periféricos:** Teclado Matricial, Sensor RFID RC522, Sensor Huella AS608, Dispaly SSD1306, Motor NEMA 17

### Backend / Servidor
* **Lenguaje/Framework:** Python Flask
* **Base de Datos:** SQLite
* **Protocolos:** TCP, HTTP

## 🚀 Instalación y Uso

1.  **Firmware:** Compilar y flashear el código en la carpeta `/firmware` utilizando [IDE o Toolchain].
2.  **Servidor:** Navegar a `/backend`, instalar dependencias y ejecutar el servicio.

---
