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
  Serial.println("Ingrese el Numero del ID de la huella a guardar (1 -127): ");
  int my_id = 0;
  
  while (true) {
    while (! Serial.available());

    char c = Serial.read();
    
    if (! isdigit(c)) break;
    my_id *= 10;
    my_id += c - '0';

    Serial.print("c: "); Serial.println(c);
    Serial.print("my_id: "); Serial.println(c);
  }

  Serial.print("Enrolando ID #");
  Serial.println(my_id);

  while (SDACTILAR_Enrolar(my_id));
}


boolean SDACTILAR_Enrolar(int id) 
{
  int p = -1;
  Serial.println("INGRESANDO");
  Serial.println("Esperando una huella valida para enrolar");
  
  while (p != SDACTILAR_OK) {
    p = MySensorDactilar.CapturarImagen();
    switch (p) {
    case SDACTILAR_OK:
      Serial.println(" ");
      Serial.println("Imagen Tomada");
      break;
    case SDACTILAR_D_NO_DETECTADO:
      Serial.print(".");
      //Serial.println("No se encuentra al dedo");
      break;
    case SDACTILAR_PAQUETE_IN_ERROR:
      Serial.println("Error al recibir el paquete");
      break;
    case SDACTILAR_IMG_ERROR:
      Serial.println("Error al determinar la imagen");
      break;
    default:
      Serial.print("Error Desconocido: 0x"); Serial.println(p, HEX);
      break;
    }
  }

  // OK success!
  p = -1; 
  p = MySensorDactilar.GenerarImg2Tz(1);
  switch (p) {
    case SDACTILAR_OK:
      Serial.println("Imagen Convertida");
      break;
    case SDACTILAR_IMGCONFUSA:
      Serial.println("Image muy confusa");
      return false; 
    case SDACTILAR_PAQUETE_IN_ERROR:
      Serial.println("Paquetes Errados");
      return false; 
    case SDACTILAR_RASGOSERROR:
      Serial.println("No es posible detectar los rasgos caracteriticos");
      return false; 
    case SDACTILAR_IMGINVALIDA:
      Serial.println("Imagen invalida");
      return false; 
    default:
      Serial.print("Error Desconocido: 0x"); Serial.println(p, HEX);
      return false; 
  }
//------------------------------------
  Serial.println("Remover Pulgar");
  delay(2000);
  p = -1;
  while (p != SDACTILAR_D_NO_DETECTADO) {
    p = MySensorDactilar.CapturarImagen();
  }
  p = -1;
  Serial.println("Por favor vuelva a poner nuevamente el Pulgar");
 
  while (p != SDACTILAR_OK) {
    p = MySensorDactilar.CapturarImagen();
    switch (p) {
    case SDACTILAR_OK:
      Serial.println(" ");
      Serial.println("Imagen Tomada");
      break;
    case SDACTILAR_D_NO_DETECTADO:
      Serial.print(".");
      //Serial.println("No se encuentra al dedo");
      break;
    case SDACTILAR_PAQUETE_IN_ERROR:
      Serial.println("Error al recibir el paquete");
      break;
    case SDACTILAR_IMG_ERROR:
      Serial.println("Error al determinar la imagen");
      break;
    default:
      Serial.print("Error Desconocido: 0x"); Serial.println(p, HEX);
      break;
    }
  }
  // OK success!
  p = -1;
  p = MySensorDactilar.GenerarImg2Tz(2);
  switch (p) {
    case SDACTILAR_OK:
      Serial.println("Imagen Convertida");
      break;
    case SDACTILAR_IMGCONFUSA:
      Serial.println("Image muy confusa");
      return false; 
    case SDACTILAR_PAQUETE_IN_ERROR:
      Serial.println("Paquetes Errados");
      return false; 
    case SDACTILAR_RASGOSERROR:
      Serial.println("No es posible detectar los rasgos caracteriticos");
      return false; 
    case SDACTILAR_IMGINVALIDA:
      Serial.println("Imagen invalida");
      return false; 
    default:
      Serial.print("Error Desconocido: 0x"); Serial.println(p, HEX);
      return false; 
  }
  // OK converted!
  p = -1;
  p = MySensorDactilar.CrearModelo();
  if (p == SDACTILAR_OK) 
  {
    Serial.println("Muestras de Huellas Emparejadas!");
  } else if (p == SDACTILAR_PAQUETE_IN_ERROR) {
    Serial.println("Error de comunicacion");
    return false; 
  } else if (p == SDACTILAR_ENROLL_MISMATCH) {
    Serial.println("Muestras de Huellas NO Emparejadas!");
    return false; 
  } else {
    Serial.print("Error Desconocido: 0x"); Serial.println(p, HEX);
    return false; 
  }   

  Serial.print("ID "); Serial.println(id);
  p = MySensorDactilar.GuardarModelo(id);
  if (p == SDACTILAR_OK) {
    Serial.println("EXITO - Huella Guardada!");
  } else if (p == SDACTILAR_PAQUETE_IN_ERROR) {
    Serial.println("Error de comunicacion");
    return false; 
  } else if (p == SDACTILAR_ERROR_UBICACION) {
    Serial.println("No se puede ubicar en la ubicacion asignada");
    return false; 
  } else if (p == SDACTILAR_ERROR_FLASH) {
    Serial.println("Error escribiendo en la flash");
    return false; 
  } else {
    Serial.print("Error Desconocido: 0x"); Serial.println(p, HEX);
    return false; 
  }   
  return false; 
}




