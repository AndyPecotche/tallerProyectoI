
#if (ARDUINO >= 100)
 #include "Arduino.h"
 #include <HardwareSerial.h>
#endif

#define SDACTILAR_OK 0x00
#define SDACTILAR_PAQUETE_IN_ERROR 0x01
#define SDACTILAR_D_NO_DETECTADO 0x02
#define SDACTILAR_IMG_ERROR 0x03
#define SDACTILAR_IMGCONFUSA 0x06
#define SDACTILAR_RASGOSERROR 0x07
#define SDACTILAR_ENROLL_MISMATCH 0x0A
#define SDACTILAR_ERROR_UBICACION 0x0B
#define SDACTILAR_IMGINVALIDA 0x15
#define SDACTILAR_ERROR_FLASH 0x18
#define SDACTILAR_CLAVEMODIFICADA 0x00
#define SDACTILAR_PAQUETE_ERROR 0x01
#define SDACTILAR_CAMBIAR_CLAVE 0x12
#define SDACTILAR_VERIFICARCLAVE 0x13
#define SDACTILAR_CAPTURARIMG 0x01
#define SDACTILAR_GENERARIMG2TZ 0x02
#define SDACTILAR_BD_OK 0x00
#define SDACTILAR_BD_PAQUETE_ERROR 0x01
#define SDACTILAR_BD_ERROR 0x11


#define FINGERPRINT_NOMATCH 0x08
#define FINGERPRINT_NOTFOUND 0x09
#define FINGERPRINT_DBRANGEFAIL 0x0C
#define FINGERPRINT_UPLOADFEATUREFAIL 0x0D
#define FINGERPRINT_PACKETRESPONSEFAIL 0x0E
#define FINGERPRINT_UPLOADFAIL 0x0F
#define FINGERPRINT_DELETEFAIL 0x10
#define FINGERPRINT_DBCLEARFAIL 0x11
#define FINGERPRINT_PASSFAIL 0x13
#define FINGERPRINT_INVALIDREG 0x1A
#define FINGERPRINT_ADDRCODE 0x20
#define FINGERPRINT_PASSVERIFY 0x21
#define FINGERPRINT_STARTCODE 0xEF01
#define FINGERPRINT_COMMANDPACKET 0x1
#define FINGERPRINT_DATAPACKET 0x2
#define FINGERPRINT_ACKPACKET 0x7
#define FINGERPRINT_ENDDATAPACKET 0x8
#define FINGERPRINT_TIMEOUT 0xFF
#define FINGERPRINT_BADPACKET 0xFE

#define FINGERPRINT_REGMODEL 0x05
#define FINGERPRINT_STORE 0x06
#define FINGERPRINT_LOAD 0x07
#define FINGERPRINT_UPLOAD 0x08
#define FINGERPRINT_DELETE 0x0C
#define FINGERPRINT_EMPTY 0x0D

#define FINGERPRINT_HISPEEDSEARCH 0x1B
#define FINGERPRINT_TEMPLATECOUNT 0x1D



#define DEFAULTTIMEOUT 5000  // milliseconds


class InnovaS_Dactilar {
 public:
#if ARDUINO >= 100
  InnovaS_Dactilar(HardwareSerial *,uint32_t);
#endif

  void begin(uint16_t baud);

  boolean VerificarClave(void);
  uint8_t CambiarClave(uint32_t password);
  uint8_t CapturarImagen(void);
  uint8_t GenerarImg2Tz(uint8_t slot = 1);
  uint8_t CrearModelo(void);

  uint8_t VaciarBaseDatos(void);
  uint8_t GuardarModelo(uint16_t id);
  uint8_t loadModel(uint16_t id);
  uint8_t getModel(void);
  uint8_t EliminarModelo(uint16_t id);
  uint8_t BusquedaRapida(void);
  uint8_t ContarRegistrados(void);
  
  void writePacket(uint8_t packettype, uint16_t len, uint8_t *packet);
  int getReply(uint8_t OpcionSelect);
  uint8_t getReply2(uint8_t packet[]);

  uint16_t Id_Dactilar, Seguridad;

 private: 
  uint32_t SD_Clave;
  uint32_t theAddress;
#if ARDUINO >= 100
  HardwareSerial *mySerial;
#endif
};
