#include "datapath.h"

void set_control_signal(uint8_t opcode, uint8_t funct, control_signal *control){
    memset(control, 0, sizeof(control_signal));

    switch (opcode)
    {
    /*────────── R‑type ─────────*/
    case 0x00:
        if (funct == 0x08)
        {
            control->JumpRegister = 1; /* jr */
        }
        else if (funct == 0x09)
        {
            control->JumpRegister = 1; /* jalr */
            control->RegDst = 1;
            control->RegWrite = 1;
        }
        else
        {
            control->RegDst = 1;
            control->RegWrite = 1;
            control->ALUOp = 2; /* use funct */
        }
        break;

    /*────────── J‑type ─────────*/
    case 0x02: /* j   */
        control->Jump = 1;
        break;
    case 0x03: /* jal */
        control->Jump = 1;
        control->RegWrite = 1;
        break;

    /*────────── Memory --------*/
    case 0x23: /* lw  */
        control->ALUSrc = 1;
        control->MemRead = 1;
        control->MemToReg = 1;
        control->RegWrite = 1;
        break;
    case 0x2B: /* sw  */
        control->ALUSrc = 1;
        control->MemWrite = 1;
        break;

    /*────────── Branch --------*/
    case 0x04:
    case 0x05: /* beq / bne */
        control->Branch = 1;
        control->ALUOp = 1; /* subtraction */
        break;

    /*────────── Immediate ALU – default add/logic */
    case 0x0E: /* xori */
        control->ALUSrc = 1;
        control->RegWrite = 1;
        control->ALUOp = 3; /* XOR immediate special */
        break;
    case 0x08:
    case 0x09: /* addi / addiu */
    case 0x0A:
    case 0x0B: /* slti / sltiu */
    case 0x0C:
    case 0x0D: /* andi / ori   */
    case 0x0F: /* lui          */
        control->ALUSrc = 1;
        control->RegWrite = 1;
        break;

    default:
        /* Other opcodes not in assignment scope → treated as NOP */
        break;
    }
}

uint8_t alu_signal(uint8_t ALUOp, uint8_t funct)
{
    if (ALUOp == 0)
        return ALU_ADD; /* default add */
    if (ALUOp == 1)
        return ALU_SUB; /* beq/bne     */
    if (ALUOp == 3)
        return ALU_XOR; /* xori        */

    /* ALUOp == 2 → look at funct */
    switch (funct)
    {
    case 0x20:
    case 0x21:
        return ALU_ADD;
    case 0x22:
    case 0x23:
        return ALU_SUB;
    case 0x24:
        return ALU_AND;
    case 0x25:
        return ALU_OR;
    case 0x26:
        return ALU_XOR;
    case 0x27:
        return ALU_NOR;
    case 0x2A:
        return ALU_SLT;
    case 0x2B:
        return ALU_SLTU;
    case 0x00:
        return ALU_SLL;
    case 0x02:
        return ALU_SRL;
    case 0x03:
        return ALU_SRA;
    case 0x04:
        return ALU_SLLV;
    case 0x06:
        return ALU_SRLV;
    case 0x07:
        return ALU_SRAV;
    default:
        return ALU_ADD;
    }
}