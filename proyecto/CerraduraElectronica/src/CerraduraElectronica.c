#include "sapi.h"
#include "MEF.h"

int main(void){
	boardConfig();
	display_init();
	driverConfig();
	tecladoInit();
    mefInit();
    while(true){
        mefUpdate();
    }

    return 0;
}
