#include "sapi.h"
#include "MEF.h"

int main(void){
    mefInit();
    driverConfig();
    while(true){
        mefUpdate();
    }

    return 0;
}
