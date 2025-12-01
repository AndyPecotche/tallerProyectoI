/*
Innova Domotics – Cursos
https://cursos.innovadomotics.com/cursos-innova
INTERNET DE LAS COSAS - IOT
Cursos IoT – Completo, Grafana, Node.js, Node Red, Arduino
https://cursos.innovadomotics.com/cursos/internet-de-las-cosas-monitoreo-y-control-back-end
Cursos IoT – Grafana, Arduino
https://cursos.innovadomotics.com/cursos/iot-grafana-monit-mysql/
Cursos IoT – Node Red, Arduino
https://cursos.innovadomotics.com/cursos/internet-de-las-cosas-node-red/

Asesoría
Arduino, Arduino Web, Raspberry
https://cursos.innovadomotics.com/asesoria/    Contacto al final de la Pagina
-	Desarrollo para dispositivos móviles Android.
-	Controles Industriales
-	Integraciones con microcontroladores.
-	Aplicaciones del Internet de las Cosas IOT, Front-end y back-end.
-	Visión Artificial, Open CV, Python.
-	Desarrollos Raspberry PI.
-	Controles con Pantallas Nextion.
-	Aplicaciones de Domótica y Robótica.
*/
//--------------------------------------------------------------------
//--------------------------------------------------------------------
#include <InnovaS_Dactilar.h>
#include <HardwareSerial.h>

HardwareSerial MySerial(2);

uint32_t ClaveSensorHuella = 0;
InnovaS_Dactilar MySensorDactilar = InnovaS_Dactilar(&MySerial, ClaveSensorHuella);

void setup()
{ 
    Serial.begin(57600);
    Serial.println("Sensor de Huella");
  
    //Setea la velocidad de comunicacion con el sensor de huella
    //Iniciar verificando los valores de 9600 y 57600
    MySensorDactilar.begin(57600);
    
    if (MySensorDactilar.VerificarClave()) {
      Serial.println("Sensor de Huella Encontrado :)  :) ");
    } else {
      Serial.println("No fue posible encontrar al sensor de Huella  :(  :( ");
      while (1);
    }

    byte TotalRegistrados = (byte)MySensorDactilar.ContarRegistrados();
    Serial.print("El sensor tiene "); Serial.print(TotalRegistrados); Serial.println(" usuarios registrados");

    EliminarBaseDatos();
}

void loop()                     
{ 
  delay(500);
}

void EliminarBaseDatos (void)
{
    uint8_t p = MySensorDactilar.VaciarBaseDatos();

    switch (p) {
    case SDACTILAR_BD_OK:
      Serial.println("Exito - Base de Datos Eliminada");
      break;
    case SDACTILAR_BD_PAQUETE_ERROR:
      Serial.print("Error al recibir el paquete");
      break;
    case SDACTILAR_BD_ERROR:
      Serial.print("Error al eliminar");
      break;
    default:
      Serial.print("Error Desconocido: 0x"); Serial.println(p, HEX);
      break;
    }
}

