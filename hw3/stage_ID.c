#include "structure.h"

void stage_ID(){
    printf("ID stage");
    
    // IF/ID latch가 유효하진 확인
    if(!id_ex_latch.valid){
        printf("No valid instruction in ID stage\n");
        id_ex_latch.valid = false;
        return;
    }
    
}