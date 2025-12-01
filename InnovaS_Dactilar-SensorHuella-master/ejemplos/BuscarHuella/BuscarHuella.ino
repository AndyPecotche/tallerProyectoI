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
}

void loop()                     
{ 
   BuscarID_Huella();
   delay(100);  
}

void BuscarID_Huella(void) 
{
  int p = -1;
  p = MySensorDactilar.CapturarImagen();
  if (p != SDACTILAR_OK)  return;

  p = MySensorDactilar.GenerarImg2Tz();
  if (p != SDACTILAR_OK)  return;

  p = MySensorDactilar.BusquedaRapida();
  if (p != SDACTILAR_OK)  return;
  
  Serial.print("Encontrado ID #"); Serial.print(MySensorDactilar.Id_Dactilar); 
  Serial.print(" con seguridad: "); Serial.println(MySensorDactilar.Seguridad);
}

