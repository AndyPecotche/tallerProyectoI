#include "mef_config.h"
#include "sapi.h"

void configurarPines(void){
    Chip_SCU_PinMux(0, 1, SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS, FUNC0);
    Chip_GPIO_SetPinDIRInput(LPC_GPIO_PORT, SENSORHALL_GPIO_PORT, SENSORHALL_GPIO_PIN);
}

void configurarInterrupcionPRESENCIA(void){
    Chip_PININT_Init(LPC_GPIO_PIN_INT);
	Chip_SCU_PinMux(1, 17, SCU_MODE_PULLDOWN | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS, FUNC0);
	Chip_GPIO_SetPinDIRInput(LPC_GPIO_PORT, 0, 12);
	Chip_SCU_GPIOIntPinSel(0, 0, 12); // PRESENCIA -> GPIO0[12]
    Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH0);
    Chip_PININT_SetPinModeEdge(LPC_GPIO_PIN_INT, PININTCH0);
    Chip_PININT_EnableIntHigh(LPC_GPIO_PIN_INT, PININTCH0);
    //NVIC_ClearPendingIRQ(PIN_INT0_IRQn);
    //NVIC_EnableIRQ(PIN_INT0_IRQn);
}

void configurarInterrupcionRFID(void){
    Chip_PININT_Init(LPC_GPIO_PIN_INT);
	Chip_SCU_PinMux(4, 5, SCU_MODE_PULLUP | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS, FUNC0);
	Chip_GPIO_SetPinDIRInput(LPC_GPIO_PORT, 2, 5);
	Chip_SCU_GPIOIntPinSel(1, 2, 5); // RFID -> GPIO2[5]
    Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH1);
    Chip_PININT_SetPinModeEdge(LPC_GPIO_PIN_INT, PININTCH1);
    Chip_PININT_EnableIntLow(LPC_GPIO_PIN_INT, PININTCH1);
    NVIC_ClearPendingIRQ(PIN_INT1_IRQn);
    NVIC_EnableIRQ(PIN_INT1_IRQn);
}
