#include "sapi.h"
#include "MEF.h"

int main(void){
    mefInit();
    while(true){
        mefUpdate();
    }

    return 0;
}
