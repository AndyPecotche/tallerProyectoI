// #ifndef RC522_H_
// #define RC522_H_

// #include "sapi.h"

// // SDA (Chip Select) -> GPIO6
// // RST (Reset)       -> GPIO7
// #define RC522_SS_PIN    GPIO6
// #define RC522_RST_PIN   GPIO7

// // --- REGISTROS MFRC522 (Direcciones) ---
// #define CommandReg    0x01
// #define ComIEnReg     0x02
// #define DivIEnReg     0x03
// #define CommIrqReg    0x04
// #define DivIrqReg     0x05
// #define ErrorReg      0x06
// #define Status1Reg    0x07
// #define Status2Reg    0x08
// #define FIFODataReg   0x09
// #define FIFOLevelReg  0x0A
// #define WaterLevelReg 0x0B
// #define ControlReg    0x0C
// #define BitFramingReg 0x0D
// #define CollReg       0x0E
// #define ModeReg       0x11
// #define TxModeReg     0x12
// #define RxModeReg     0x13
// #define TxControlReg  0x14
// #define TxASKReg      0x15
// #define ModWidthReg   0x24
// #define VersionReg    0x37

// // --- COMANDOS ---
// #define PCD_IDLE       0x00
// #define PCD_AUTHENT    0x0E
// #define PCD_RECEIVE    0x08
// #define PCD_TRANSMIT   0x04
// #define PCD_TRANSCEIVE 0x0C
// #define PCD_RESETPHASE 0x0F
// #define PCD_CALCCRC    0x03

// #define PICC_REQIDL    0x26
// #define PICC_ANTICOLL  0x93

// #define MI_OK          0
// #define MI_NOTAGERR    1
// #define MI_ERR         2

// // --- PROTOTIPOS P�BLICOS ---
// void PCD_Init(void);
// void PCD_WriteRegister(uint8_t reg, uint8_t value);
// uint8_t PCD_ReadRegister(uint8_t reg);
// void PCD_SetAntennaOn(bool_t on);

// // Funciones de alto nivel para detecci�n
// bool_t PICC_IsNewCardPresent(void);
// bool_t PICC_ReadCardSerial(void);
// void PICC_HaltA(void);

// // Variables externas para acceder al UID le�do
// extern uint8_t uid[10];
// extern uint8_t uidSize;

// // Constantes para obtener bytes del UID f�cilmente
// uint8_t PICC_GetUidByte(uint8_t index);
// // En rc522.h
// void PCD_ActivarIRQ(void);
// void PCD_LimpiarIRQ(void);
// void PCD_StopCrypto1(void);
// #endif /* RC522_H_ */
