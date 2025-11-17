#include "sapi.h"
#include "mef.h"

int main(void){
    mefInit();

    while(true){
        mefUpdate();
    }

    return 0;
}
