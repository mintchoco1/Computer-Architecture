#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#define MEM_SIZE 0x1000000

#define ALU_AND 0
#define ALU_OR 1
#define ALU_ADD 2
#define ALU_XOR 3
#define ALU_SUB 6
#define ALU_SLT 7 /* signed */
#define ALU_SLL 8
#define ALU_SRL 9
#define ALU_SRA 10
#define ALU_SLTU 11 /* unsigned */
#define ALU_NOR 12
#define ALU_SLLV 13
#define ALU_SRLV 14
#define ALU_SRAV 15

typedef struct instruction{
    uint32_t raw_bits;
    uint8_t opcode;
    uint8_t rs;
    uint8_t rt;
    uint8_t rd;
    uint8_t shamt;
    uint8_t funct;
    int16_t immediate;
    uint32_t address;
} instruction;

typedef struct control_signal{
    int RegDst;
    int ALUSrc;
    int MemToReg;
    int RegWrite;
    int MemRead;
    int MemWrite;
    int Branch;
    int Jump;
    int JumpRegister;
    uint8_t ALUOp;
} control_signal;
