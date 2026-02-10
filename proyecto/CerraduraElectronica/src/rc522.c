// #include "rc522.h"
// #include <string.h>

// // Variables globales
// uint8_t uid[10];
// uint8_t uidSize;

// // --- FUNCIONES SPI DE BAJO NIVEL ---

// static void RC522_WriteReg(uint8_t addr, uint8_t val) {
//     uint8_t buffer[2];
//     buffer[0] = (addr << 1) & 0x7E; // Direcci�n formateada para escritura
//     buffer[1] = val;                // Dato a escribir

//     gpioWrite(RC522_SS_PIN, OFF);   // Seleccionar (Bajar SS)

//     // Enviamos los 2 bytes juntos (Direcci�n + Dato)
//     spiWrite(SPI0, buffer, 2);

//     gpioWrite(RC522_SS_PIN, ON);    // Deseleccionar (Subir SS)
// }

// static uint8_t RC522_ReadReg(uint8_t addr) {
//     uint8_t val = 0;
//     uint8_t addrByte = ((addr << 1) & 0x7E) | 0x80; // Direcci�n formateada para lectura

//     gpioWrite(RC522_SS_PIN, OFF);   // Seleccionar

//     spiWrite(SPI0, &addrByte, 1);   // 1. Enviar direcci�n
//     spiRead(SPI0, &val, 1);         // 2. Leer 1 byte de respuesta

//     gpioWrite(RC522_SS_PIN, ON);    // Deseleccionar
//     return val;
// }

// // Wrappers p�blicos
// void PCD_WriteRegister(uint8_t reg, uint8_t value) {
//     RC522_WriteReg(reg, value);
// }

// uint8_t PCD_ReadRegister(uint8_t reg) {
//     return RC522_ReadReg(reg);
// }

// static void PCD_SetAntennaGain(void) {
//     // Registro RFCfgReg (0x26)
//     // Ponemos los bits de RxGain al m�ximo (48 dB) -> 0x07 << 4
//     uint8_t current = PCD_ReadRegister(0x26); // RFCfgReg
//     PCD_WriteRegister(0x26, (current & 0x8F) | 0x70);
//     printf("[RFID] Ganancia de antena ajustada al maximo (48dB)\r\n");
// }

// void PCD_SetAntennaOn(bool_t on) {
//     uint8_t temp = PCD_ReadRegister(TxControlReg);
//     if (on) {
//         if ((temp & 0x03) != 0x03) {
//             PCD_WriteRegister(TxControlReg, temp | 0x03);
//         }
//     } else {
//         PCD_WriteRegister(TxControlReg, temp & (~0x03));
//     }
// }

// // --- INICIALIZACI�N ---
// void PCD_Init(void) {
//     // Configurar Pines de Control
//     gpioConfig(RC522_SS_PIN, GPIO_OUTPUT);
//     gpioConfig(RC522_RST_PIN, GPIO_OUTPUT);

//     gpioWrite(RC522_SS_PIN, ON); // Deshabilitado (High)

//     // Configurar SPI0
//     spiConfig(SPI0);

//     // Hard Reset
//     gpioWrite(RC522_RST_PIN, ON);
//     delay(10);
//     gpioWrite(RC522_RST_PIN, OFF); // Reset activo
//     delay(10);
//     gpioWrite(RC522_RST_PIN, ON);  // Reset liberado
//     delay(50);

//     // Soft Reset
//     PCD_WriteRegister(CommandReg, PCD_RESETPHASE);
//     delay(50);

//     // Configuraci�n b�sica del chip
//     PCD_WriteRegister(ModeReg, 0x3D);       // Definir timer prescaler
//     PCD_WriteRegister(TxASKReg, 0x40);      // Modulaci�n 100% ASK
//     PCD_WriteRegister(ControlReg, 0x10);    // Activar timer

//     PCD_SetAntennaOn(TRUE); // Encender antena
//     PCD_SetAntennaGain(); // Fuerza m�xima potencia
// }

// // --- COMUNICACI�N CON TARJETA ---
// static uint8_t PCD_ToCard(uint8_t command, uint8_t *sendData, uint8_t sendLen, uint8_t *backData, uint8_t *backLen) {
//     uint8_t status = MI_ERR;
//     uint8_t irqEn = 0x00;
//     uint8_t waitIRq = 0x00;
//     uint8_t n;
//     uint32_t i;

//     if (command == PCD_AUTHENT) {
//         irqEn = 0x12;
//         waitIRq = 0x10;
//     } else if (command == PCD_TRANSCEIVE) {
//         irqEn = 0x77;
//         waitIRq = 0x30;
//     }

//     PCD_WriteRegister(ComIEnReg, irqEn | 0x80);
//     PCD_WriteRegister(CommIrqReg, 0x7F); // Limpiar bits de interrupci�n
//     PCD_WriteRegister(FIFOLevelReg, 0x80); // Flush FIFO
//     PCD_WriteRegister(CommandReg, PCD_IDLE);

//     // Escribir datos al FIFO
//     for (i = 0; i < sendLen; i++) {
//         PCD_WriteRegister(FIFODataReg, sendData[i]);
//     }

//     PCD_WriteRegister(CommandReg, command);
//     if (command == PCD_TRANSCEIVE) {
//         PCD_WriteRegister(BitFramingReg, 0x80); // StartSend
//     }

//     // Esperar a que termine el comando (por IRQ o Timeout)
//     i = 2000;
//     while (1) {
//         n = PCD_ReadRegister(CommIrqReg);
//         if (n & waitIRq) break;         // Terminado
//         if (n & 0x01) return MI_ERR;    // Timer interrupt (Timeout)
//         if (--i == 0) return MI_ERR;    // Software timeout
//     }

//     uint8_t errorRegValue = PCD_ReadRegister(ErrorReg);
//     if (!(errorRegValue & 0x1B)) {
//         status = MI_OK;
//         if (n & irqEn & 0x01) status = MI_NOTAGERR;

//         if (command == PCD_TRANSCEIVE) {
//             n = PCD_ReadRegister(FIFOLevelReg);
//             uint8_t lastBits = PCD_ReadRegister(ControlReg) & 0x07;
//             if (lastBits) *backLen = (n - 1) * 8 + lastBits;
//             else *backLen = n * 8;

//             if (n == 0) n = 1;
//             if (n > 16) n = 16;

//             for (i = 0; i < n; i++) {
//                 backData[i] = PCD_ReadRegister(FIFODataReg);
//             }
//         }
//     } else {
//         status = MI_ERR;
//     }

//     return status;
// }

// bool_t PICC_IsNewCardPresent(void) {
//     uint8_t bufferATQA[2];
//     uint8_t bufferSize = sizeof(bufferATQA);

//     // Reset baud rates
//     PCD_WriteRegister(TxModeReg, 0x00);
//     PCD_WriteRegister(RxModeReg, 0x00);
//     PCD_WriteRegister(ModWidthReg, 0x26);

//     // RequestA (Busca tarjetas en el campo)
//     uint8_t result = PCD_ToCard(PCD_TRANSCEIVE, (uint8_t[]){PICC_REQIDL}, 1, bufferATQA, &bufferSize);
//     return (result == MI_OK || result == 0x0E); // 0x0E es colisi�n (hay tarjeta)
// }

// bool_t PICC_ReadCardSerial(void) {
//     uint8_t status;
//     uint8_t buffer[18];
//     uint8_t len;

//     // Clear UID
//     memset(uid, 0, sizeof(uid));
//     uidSize = 0;

//     PCD_WriteRegister(BitFramingReg, 0x00);

//     // Anticollision Loop
//     uint8_t serNum[2];
//     serNum[0] = PICC_ANTICOLL;
//     serNum[1] = 0x20;

//     status = PCD_ToCard(PCD_TRANSCEIVE, serNum, 2, buffer, &len);

//     if (status == MI_OK) {
//         uidSize = 4; // Asumimos UID est�ndar de 4 bytes
//         memcpy(uid, buffer, 4);
//         return TRUE;
//     }
//     return FALSE;
// }

// void PICC_HaltA(void) {
//     uint8_t buffer[4];
//     uint8_t len;
//     buffer[0] = 0x50; // HLTA cmd
//     buffer[1] = 0x00;
//     PCD_ToCard(PCD_TRANSMIT, buffer, 2, buffer, &len);
// }

// // Helper para obtener bytes del UID de forma segura
// uint8_t PICC_GetUidByte(uint8_t index){
//     if(index < uidSize) return uid[index];
//     return 0;
// }


// void PCD_ActivarIRQ(void) {
//     // 1. Limpiar bits de interrupci�n previos
//     PCD_WriteRegister(CommIrqReg, 0x7F);

//     // 2. Configurar Interrupciones:
//     // Bit 7 (0x80) = IRQInv: Invierte la se�al (0 = Activo, para que baje el pin)
//     // Bit 5 (0x20) = RxIEn:  Activa IRQ cuando el receptor detecta algo
//     // Total = 0xA0
//     PCD_WriteRegister(ComIEnReg, 0xA0);

//     // 3. Configurar DivIEnReg (Importante para que la se�al salga por el pin)
//     PCD_WriteRegister(DivIEnReg, 0x80);

//     // 4. Resetear FIFO
//     PCD_WriteRegister(FIFOLevelReg, 0x80);

//     // 5. Poner el chip en modo TRANSCEIVE (Transmisi�n/Recepci�n)
//     // pero sin enviar datos, para que se quede "escuchando" cambios en el campo.
//     PCD_WriteRegister(CommandReg, PCD_TRANSCEIVE);
//     PCD_WriteRegister(BitFramingReg, 0x87); // Iniciar transmisi�n (necesario para activar Rx)
// }

// void PCD_LimpiarIRQ(void) {
//     // Funci�n para "bajar la bandera" despu�s de leer
//     PCD_WriteRegister(CommIrqReg, 0x7F);
// }

// // Desactiva la encriptaci�n Crypto1 para que el lector quede limpio
// void PCD_StopCrypto1(void) {
//     // 1. Leemos el estado actual del registro 0x08 (Status2Reg)
//     // (Status2Reg es el registro que controla la encriptaci�n)
//     uint8_t valorActual = PCD_ReadRegister(0x08);

//     // 2. Apagamos el bit 3 (0x08) que corresponde a MFCrypto1On
//     // Usamos AND con el inverso de 0x08 (0xF7) para borrar solo ese bit
//     uint8_t nuevoValor = valorActual & (~0x08);

//     // 3. Escribimos el nuevo valor corregido
//     PCD_WriteRegister(0x08, nuevoValor);
// }
