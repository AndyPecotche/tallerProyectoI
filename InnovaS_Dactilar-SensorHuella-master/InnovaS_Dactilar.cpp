
#include "InnovaS_Dactilar.h"
// #include <util/delay.h>

#if (ARDUINO >= 100)
  #include <HardwareSerial.h>
#endif

InnovaS_Dactilar::InnovaS_Dactilar(HardwareSerial *ss, uint32_t MyClave) 
{
  SD_Clave = MyClave;
  theAddress = 0xFFFFFFFF;
  mySerial = ss;
}

/*
InnovaS_Dactilar::InnovaS_Dactilar(HardwareSerial *ss) 
{
  //CLAVE SENSOR DE HUELLA
  SD_Clave = 0;
  theAddress = 0xFFFFFFFF;
  mySerial = ss;
}
*/

void InnovaS_Dactilar::begin(uint16_t baudrate) {
  delay(1000);  // one second delay to let the sensor 'boot up'
  mySerial->begin(baudrate);
}

boolean InnovaS_Dactilar::VerificarClave(void) {
  
  // uint8_t packet[] = {SDACTILAR_VERIFICARCLAVE, (SD_Clave >> 24), (SD_Clave >> 16),(SD_Clave >> 8), SD_Clave};

   uint8_t packet[] = {SDACTILAR_VERIFICARCLAVE, (uint8_t)(SD_Clave >> 24), (uint8_t)(SD_Clave >> 16),
  (uint8_t)(SD_Clave >> 8), (uint8_t)(SD_Clave & 0xFF)};

  writePacket(FINGERPRINT_COMMANDPACKET, sizeof(packet) + 2, packet);

  int Respuesta = getReply(1);
  
  if(Respuesta == 0x00 ){Serial.println("Clave Correcta");return true;}
  if(Respuesta == 0x01 ){Serial.println("Error al recibir el paquete");}
  if(Respuesta == 0x13 ){Serial.println("Clave INCORRECTA");}
  return false;
}

uint8_t InnovaS_Dactilar::CambiarClave(uint32_t password) 
{
  uint8_t packet[] = {SDACTILAR_CAMBIAR_CLAVE, (password >> 24), (password >> 16), (password >> 8), password};
  writePacket(FINGERPRINT_COMMANDPACKET, sizeof(packet) + 2, packet);
  return getReply(1);
}


uint8_t InnovaS_Dactilar::CapturarImagen(void) 
{
  uint8_t packet[] = {SDACTILAR_CAPTURARIMG};
  writePacket(FINGERPRINT_COMMANDPACKET, sizeof(packet) + 2, packet);
  return getReply(1);
}


uint8_t InnovaS_Dactilar::GenerarImg2Tz(uint8_t slot) {
  uint8_t packet[] = {SDACTILAR_GENERARIMG2TZ, slot};
  
  writePacket(FINGERPRINT_COMMANDPACKET, sizeof(packet)+2, packet);
  return getReply(1);
}

uint8_t InnovaS_Dactilar::CrearModelo(void) 
{
  uint8_t packet[] = {FINGERPRINT_REGMODEL}; 
  writePacket(FINGERPRINT_COMMANDPACKET, sizeof(packet)+2, packet);
  return getReply(1);
}

uint8_t InnovaS_Dactilar::GuardarModelo(uint16_t id) {
  uint8_t packet[] = {FINGERPRINT_STORE, 0x01, id >> 8, id & 0xFF};
  writePacket(FINGERPRINT_COMMANDPACKET, sizeof(packet)+2, packet);
  return getReply(1); 
}

uint8_t InnovaS_Dactilar::EliminarModelo(uint16_t id) {
    uint8_t packet[] = {FINGERPRINT_DELETE, id >> 8, id & 0xFF, 0x00, 0x01};
    
    writePacket(FINGERPRINT_COMMANDPACKET, sizeof(packet)+2, packet);
    uint8_t len = getReply2(packet);
        
    if ((len != 1) && (packet[0] != FINGERPRINT_ACKPACKET))
        return -1;
    return packet[1];
}

uint8_t InnovaS_Dactilar::VaciarBaseDatos(void) {
  uint8_t packet[] = {FINGERPRINT_EMPTY};
  writePacket(FINGERPRINT_COMMANDPACKET, sizeof(packet)+2, packet);
  uint8_t len = getReply2(packet);
  
  if ((len != 1) && (packet[0] != FINGERPRINT_ACKPACKET))
  return packet[1];
}

uint8_t InnovaS_Dactilar::BusquedaRapida(void) {
  Id_Dactilar = 0xFFFF;
  Seguridad = 0xFFFF;
  // high speed search of slot #1 starting at page 0x0000 and page #0x00A3 
  uint8_t packet[] = {FINGERPRINT_HISPEEDSEARCH, 0x01, 0x00, 0x00, 0x00, 0xA3};
  writePacket(FINGERPRINT_COMMANDPACKET, sizeof(packet)+2, packet);
  uint8_t len = getReply2(packet);
  
  if ((len != 1) && (packet[0] != FINGERPRINT_ACKPACKET))
   return -1;

  Id_Dactilar = packet[2];
  Id_Dactilar <<= 8;
  Id_Dactilar |= packet[3];
  
  Seguridad = packet[4];
  Seguridad <<= 8;
  Seguridad |= packet[5];
  
  return packet[1];
}


uint8_t InnovaS_Dactilar::ContarRegistrados(void) {

  uint8_t packet[] = {FINGERPRINT_TEMPLATECOUNT};

  writePacket(FINGERPRINT_COMMANDPACKET, sizeof(packet) + 2, packet);
 
  return getReply(2);
}


void InnovaS_Dactilar::writePacket(uint8_t packettype, uint16_t len, uint8_t *packet) 
{
  //INICIO 2
  //DIRECCION 4
  //IDENTIFICADOR DEL PAQUETE 1
  //LARGO DEL PAQUETE 2
  //PAQUETE DE DATOS XXX
  //CHECK SUM 2

  //.....................................................................
  // Serial.print("Longitud: "); Serial.println(len);
  //.....................................................................
  //INICIO
  mySerial->write((uint8_t)(FINGERPRINT_STARTCODE >> 8));
  mySerial->write((uint8_t)FINGERPRINT_STARTCODE);
  //DIRECCION
  mySerial->write((uint8_t)(theAddress >> 24));
  mySerial->write((uint8_t)(theAddress >> 16));
  mySerial->write((uint8_t)(theAddress >> 8));
  mySerial->write((uint8_t)(theAddress));
  //IDENTIFICADOR DEL PAQUETE
  mySerial->write((uint8_t)packettype);
  //LARGO DEL PAQUETE
  mySerial->write((uint8_t)(len >> 8));
  mySerial->write((uint8_t)(len));
  
  uint16_t sum = (len>>8) + (len&0xFF) + packettype;
  //PAQUETE DE DATOS
  for (uint8_t i=0; i< len-2; i++) 
  {
    mySerial->write((uint8_t)(packet[i]));
    sum += packet[i];
  }
  //CHECK SUM
  mySerial->write((uint8_t)(sum>>8));
  mySerial->write((uint8_t)sum);

}

int InnovaS_Dactilar::getReply(uint8_t OpcionSelect) 
{
  uint8_t reply[20], idx;
  uint16_t timer=0;
  
  idx = 0;

while (true) {
    while (!mySerial->available()) {
      delay(1);
      timer++;
      if (timer >= DEFAULTTIMEOUT) return FINGERPRINT_TIMEOUT;
    }
    // something to read!
    reply[idx] = mySerial->read();
    
    if ((idx == 0) && (reply[0] != (FINGERPRINT_STARTCODE >> 8)))
      continue;

    //--------------------------------------------------------------------------------------------
    //  Serial.print("idx: ");Serial.print(idx);Serial.print(" ");Serial.println(reply[idx],HEX);
    //--------------------------------------------------------------------------------------------
   
    // check packet!
    if(OpcionSelect == 1)
    {
         if(idx == 11)
         {
            uint16_t ck_in_sum =0 ;
            uint16_t ck_sum_fg =0 ;
            
            for (uint8_t i = 6; i < 10; i++){ck_in_sum += reply[i];}
            // Serial.println(ck_in_sum,HEX);
            // reply[10] =0xAB;
            // reply[11] =0xCD;
            
            ck_sum_fg = reply[10];
            ck_sum_fg = ck_sum_fg<<8;
            ck_sum_fg = ck_sum_fg | reply[11];
            
            // Serial.println(ck_sum_fg,HEX);
            if(ck_in_sum == ck_sum_fg){return reply[9];}
            return -1;      
          }             
    }

    if(OpcionSelect == 2)
    {
          if(idx == 13)
          {
            uint16_t ck_in_sum =0 ;
            uint16_t ck_sum_fg =0 ;
    
            for (uint8_t i = 6; i < 12; i++){ck_in_sum += reply[i];}
    
            // Serial.println(ck_in_sum,HEX);
    
            // reply[10] =0xAB;
            // reply[11] =0xCD;
            
            ck_sum_fg = reply[12];
            ck_sum_fg = ck_sum_fg<<8;
            ck_sum_fg = ck_sum_fg | reply[13];
    
            // Serial.println(ck_sum_fg,HEX);
    
            if(ck_in_sum == ck_sum_fg)
            {
              ck_sum_fg = reply[10];
              ck_sum_fg = ck_sum_fg<<8;
              ck_sum_fg = ck_sum_fg | reply[11];
              
              return (uint8_t)ck_sum_fg;  
            }         
            return -1;      
          }       
    }
    idx++;    
  }
}


uint8_t InnovaS_Dactilar::getReply2(uint8_t packet[]) {
  uint8_t reply[20], idx;
  uint16_t timer=0;
  
  idx = 0;

while (true) {
    while (!mySerial->available()) {
      delay(1);
      timer++;
      if (timer >= DEFAULTTIMEOUT) return FINGERPRINT_TIMEOUT;
    }
    // something to read!
    reply[idx] = mySerial->read();

    if ((idx == 0) && (reply[0] != (FINGERPRINT_STARTCODE >> 8)))
      continue;
    idx++;
    
    // check packet!
    if (idx >= 9) {
      if ((reply[0] != (FINGERPRINT_STARTCODE >> 8)) ||
          (reply[1] != (FINGERPRINT_STARTCODE & 0xFF)))
          return FINGERPRINT_BADPACKET;
      uint8_t packettype = reply[6];
      //Serial.print("Packet type"); Serial.println(packettype);
      uint16_t len = reply[7];
      len <<= 8;
      len |= reply[8];
      len -= 2;
      //Serial.print("Packet len"); Serial.println(len);
      if (idx <= (len+10)) continue;
      packet[0] = packettype;      
      for (uint8_t i=0; i<len; i++) {
        packet[1+i] = reply[9+i];
      }

      return len;
    }
  }
}



