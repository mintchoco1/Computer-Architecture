#include "datapath3.h"

#include <stdio.h>
#include <stdio.h>
#include <stdint.h>

uint8_t alu_control(uint8_t ALUOp, uint8_t funct) {
    if (ALUOp == 0b00) return 0b0010; // lw, sw, addi
    if (ALUOp == 0b01) return 0b0110; // beq, bne

    switch (funct) {
        case 0x20: return 0b0010; // add
        case 0x22: return 0b0110; // sub
        case 0x24: return 0b0000; // and
        case 0x25: return 0b0001; // or
        case 0x27: return 0b1100; // nor
        case 0x2A: return 0b0111; // slt
        case 0x00: return 0b1000; // sll
        case 0x02: return 0b1001; // srl
        default: return 0b0010;   // default add
    }
}
