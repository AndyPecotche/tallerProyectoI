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

}

void loop()                     
{ 
  Serial.println("Ingrese el numero del ID que desea eliminar: ");
  
  uint8_t my_id = 0;
  while (true) {
    while (! Serial.available());
    char c = Serial.read();
    if (! isdigit(c)) break;
    my_id *= 10;
    my_id += c - '0';
  }
  Serial.print("Eliminando ID #");
  Serial.println(my_id);
  
  EliminarUN_ID_DACTILAR(my_id);    
}

void EliminarUN_ID_DACTILAR(uint8_t id) {
  uint8_t p = -1;
  
  p = MySensorDactilar.EliminarModelo(id);

  if (p == SDACTILAR_OK) {
    Serial.println("ID Eliminado!");
  } else if (p == SDACTILAR_PAQUETE_IN_ERROR) {
    Serial.println("Error en Comunicacion");
    return;
  } else if (p == SDACTILAR_ERROR_UBICACION) {
    Serial.println("No es posible eliminar en dicha ubicacion");
    return;
  } else if (p == SDACTILAR_ERROR_FLASH) {
    Serial.println("Error escribiendo en la flash");
    return;
  } else {
    Serial.print("Error Desconocido: 0x"); Serial.println(p, HEX);
    return;
  }   
}

