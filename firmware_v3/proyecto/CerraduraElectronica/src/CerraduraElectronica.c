#include "sapi.h"
#include "mef.h"

int main(void){
    mefInit();
    driverConfig();
    while(true){
        mefUpdate();
    }

    return 0;
}
