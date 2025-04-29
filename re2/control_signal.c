#include "datapath3.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>

void control_signal(uint8_t opcode, uint8_t funct, Control *c) {
    memset(c, 0, sizeof(Control));

    if (opcode == 0x00) { // R-type
        c->RegDst = 1; c->RegWrite = 1; c->ALUOp = 0b10;
        if (funct == 0x08 || funct == 0x09) { // jr, jalr
            c->JumpReg = 1;
            c->RegWrite = (funct == 0x09); // jalr은 RegWrite 필요
        }
    } else if (opcode == 0x23) { // lw
        c->ALUSrc = 1; c->MemRead = 1; c->MemToReg = 1; c->RegWrite = 1;
    } else if (opcode == 0x2B) { // sw
        c->ALUSrc = 1; c->MemWrite = 1;
    } else if (opcode == 0x04 || opcode == 0x05) { // beq, bne
        c->Branch = 1; c->ALUOp = 0b01;
    } else if (opcode == 0x02 || opcode == 0x03) { // j, jal
        c->Jump = 1;
        if (opcode == 0x03) c->RegWrite = 1; // jal
    } else { // I-type 연산 (addi, andi, ori, slti 등)
        c->ALUSrc = 1; c->RegWrite = 1;
    }
}
