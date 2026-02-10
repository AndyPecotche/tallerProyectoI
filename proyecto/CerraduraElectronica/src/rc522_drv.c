#include "rc522_drv.h"
#include <string.h>

/* =========================================================
 * MFRC522 (RC522) ? Driver m�nimo para ISO14443-A (REQA + anticollision CL1)
 * Ordenado para que NO requiera prototipos internos: helpers primero,
 * luego las funciones p�blicas de la API declaradas en rc522_drv.h.
 * ========================================================= */

/* ---- Registros parciales MFRC522 ---- */
enum {
  CommandReg     = 0x01,
  CommIEnReg     = 0x02,
  DivIEnReg      = 0x03,
  CommIrqReg     = 0x04,
  DivIrqReg      = 0x05,
  ErrorReg       = 0x06,
  Status1Reg     = 0x07,
  Status2Reg     = 0x08,
  FIFODataReg    = 0x09,
  FIFOLevelReg   = 0x0A,
  ControlReg     = 0x0C,
  BitFramingReg  = 0x0D,
  CollReg        = 0x0E,
  ModeReg        = 0x11,
  TxModeReg      = 0x12,
  RxModeReg      = 0x13,
  TxControlReg   = 0x14,
  TxASKReg       = 0x15,
  RFCfgReg       = 0x26,
  TModeReg       = 0x2A,
  TPrescalerReg  = 0x2B,
  TReloadRegH    = 0x2C,
  TReloadRegL    = 0x2D,
  CRCResultRegH  = 0x21,
  CRCResultRegL  = 0x22
};

/* ---- Comandos PCD ---- */
#define PCD_Idle        0x00
#define PCD_Transceive  0x0C
#define PCD_SoftReset   0x0F

/* ---- Comandos PICC (ISO14443-A) ---- */
#define PICC_REQIDL     0x26

/* ----------------- Helpers privados (static) ----------------- */
static inline void rc522_cs_low (void){ gpioWrite(RC522_CS_GPIO, OFF); }
static inline void rc522_cs_high(void){ gpioWrite(RC522_CS_GPIO, ON ); }

static inline void spi_write(const uint8_t* b, uint32_t n){ spiWrite(RC522_SPI, (uint8_t*)b, n); }
static inline void spi_read (uint8_t* b, uint32_t n){ spiRead (RC522_SPI, b, n); }

static void rc522_write_reg(uint8_t reg, uint8_t val){
  uint8_t addr = (uint8_t)((reg << 1) & 0x7E); /* write */
  rc522_cs_low();  spi_write(&addr, 1); spi_write(&val, 1); rc522_cs_high();
}

static uint8_t rc522_read_reg(uint8_t reg){
  uint8_t addr = (uint8_t)(((reg << 1) & 0x7E) | 0x80); /* read */
  uint8_t v = 0;
  rc522_cs_low();  spi_write(&addr, 1); spi_read(&v, 1); rc522_cs_high();
  return v;
}

static void rc522_set_bits(uint8_t reg, uint8_t m){ rc522_write_reg(reg, (uint8_t)(rc522_read_reg(reg) |  m)); }
static void rc522_clr_bits(uint8_t reg, uint8_t m){ rc522_write_reg(reg, (uint8_t)(rc522_read_reg(reg) & ~m)); }

static void rc522_reset(void){ rc522_write_reg(CommandReg, PCD_SoftReset); delay(50); }

static void rc522_antenna_on(void){
  if ( (rc522_read_reg(TxControlReg) & 0x03) != 0x03 ) rc522_set_bits(TxControlReg, 0x03);
}

/* Transceive b�sico por FIFO + StartSend.
 * - send: bytes a transmitir
 * - back: buffer para respuesta (puede ser NULL)
 * - backBits: cantidad de bits v�lidos en la respuesta
 * Devuelve true si hubo al menos 1 byte en FIFO al finalizar (sin errores).
 */
static bool transceive(const uint8_t* send, uint8_t sendLen,
                       uint8_t* back, uint8_t* backBits){
  rc522_write_reg(CommandReg, PCD_Idle);
  rc522_write_reg(CommIrqReg, 0x7F);      /* limpiar IRQs */
  rc522_write_reg(FIFOLevelReg, 0x80);    /* flush FIFO */

  for(uint8_t i=0;i<sendLen;i++) rc522_write_reg(FIFODataReg, send[i]);

  rc522_write_reg(CommandReg, PCD_Transceive);
  rc522_set_bits(BitFramingReg, 0x80);    /* StartSend=1 */

  /* Espera RxIRq(0x20) / IdleIRq(0x10) / Timer(DivIrqReg bit0) */
  /* Use a time-based timeout to avoid long blocking loops during polling.
     50 ms is enough for card responses; adjust if needed. */
  const tick_t transceive_timeout_ms = 50;
  tick_t trans_start = tickRead();
  while ((tick_t)(tickRead() - trans_start) < transceive_timeout_ms) {
    uint8_t irq = rc522_read_reg(CommIrqReg);
    if (irq & 0x30) break; /* RxIRq or IdleIRq */
    if (rc522_read_reg(DivIrqReg) & 0x01) break; /* timer timeout */
  }

  rc522_clr_bits(BitFramingReg, 0x80);    /* StartSend=0 */

  if (rc522_read_reg(ErrorReg) & 0x1B) return false; /* BufferOvfl | ParityErr | ProtocolErr */

  uint8_t fifo     = rc522_read_reg(FIFOLevelReg);
  uint8_t lastBits = rc522_read_reg(ControlReg) & 0x07;
  uint8_t nBytes   = fifo;
  uint8_t nBits    = (lastBits) ? (uint8_t)(((fifo - 1) * 8) + lastBits) : (uint8_t)(fifo * 8);

  if (back && nBytes){ for(uint8_t i=0;i<nBytes;i++) back[i] = rc522_read_reg(FIFODataReg); }
  if (backBits) *backBits = nBits;

  return (nBytes > 0);
}

/* =================== API p�blica (rc522_drv.h) =================== */
void rc522_hw_init(void){
  gpioConfig(RC522_CS_GPIO,  GPIO_OUTPUT);
  gpioConfig(RC522_RST_GPIO, GPIO_OUTPUT);
  rc522_cs_high();
  gpioWrite(RC522_RST_GPIO, ON);

  spiInit(RC522_SPI); /* sAPI por defecto: modo 0, clock seguro */
}

void rc522_init_iso14443a(void){
  rc522_reset();
  /* Valores recomendados por NXP para ISO14443-A */
  rc522_write_reg(TModeReg,      0x8D);
  rc522_write_reg(TPrescalerReg, 0x3E);
  rc522_write_reg(TReloadRegL,   30);
  rc522_write_reg(TReloadRegH,   0);
  rc522_write_reg(TxASKReg,      0x40); /* 100% ASK */
  rc522_write_reg(ModeReg,       0x3D); /* CRC preset 0x6363 */
  rc522_write_reg(RFCfgReg,      0x70); /* ganancia RX tipica */
  /* Clear pending IRQs and enable RxIRq(0x20) | IdleIRq(0x10)
     so the MFRC522 asserts its IRQ pin when a card is detected/response */
  rc522_write_reg(CommIrqReg, 0x7F);
  rc522_write_reg(CommIEnReg, 0x30);
  rc522_antenna_on();
  delay(5);
}

uint8_t rc522_version(void){ return rc522_read_reg(0x37); /* VersionReg */ }
uint8_t rc522_txcontrol(void){ return rc522_read_reg(TxControlReg); }

bool rc522_requestA(uint8_t atqa[2]){
  rc522_write_reg(CollReg, 0x80);         /* limpiar colisiones */
  rc522_write_reg(BitFramingReg, 0x07);   /* 7 bits */
  uint8_t cmd = PICC_REQIDL; uint8_t back[3] = {0}; uint8_t backBits = 0;
  bool ok = transceive(&cmd, 1, back, &backBits);
  rc522_write_reg(BitFramingReg, 0x00);
  if (!ok || backBits != 16) return false;
  atqa[0] = back[0]; atqa[1] = back[1];
  return true;
}

bool rc522_anticoll_cl1(uint8_t uid5[5]){
  rc522_write_reg(BitFramingReg, 0x00);
  uint8_t cmd[2] = { 0x93, 0x20 }; /* ANTICOLLISION CL1, NVB=0x20 */
  uint8_t back[10]; uint8_t bits = 0;
  if (!transceive(cmd, 2, back, &bits)) return false;
  if (bits != 40) return false; /* 4 UID bytes + BCC = 40 bits */
  for (int i=0;i<5;i++) uid5[i] = back[i];
  return true;
}

/* Limpia las IRQs internas del MFRC522 (CommIrqReg) */
void rc522_clear_irqs(void){
  rc522_write_reg(CommIrqReg, 0x7F);
}

bool leer_rfid_str(char *out, size_t maxlen){
    uint8_t atqa[2];
    if (!rc522_requestA(atqa))
        return false;

    uint8_t uid5[5];
    if (!rc522_anticoll_cl1(uid5))
        return false;

    // Por ahora solo CL1 → 4 bytes
    // (después se puede extender a CL2 sin romper nada)
    uint8_t uid_len = 4;

    if (maxlen < (uid_len * 2 + 1))
        return false;

    const char *h = "0123456789ABCDEF";
    int k = 0;

    for (int i = 0; i < uid_len; i++) {
        out[k++] = h[(uid5[i] >> 4) & 0x0F];
        out[k++] = h[uid5[i] & 0x0F];
    }
    out[k] = '\0';

    return true;
}
