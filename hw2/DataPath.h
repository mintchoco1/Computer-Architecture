#include <stdio.h>
#include <stdint.h>

#define Mem_size 2048

uint32_t mem[Mem_size];

typedef struct{
    uint32_t Reg[32]; // 32 Reigsters
    int pc; // program counter
} Reigsters;

typedef struct {
    int RegDest;
    int AluSrc;
    int MemToReg;
    int RegWrite;
    int MemRead;
    int MemWrite;
} control_signal;   

typedef struct instruction {
    uint8_t opcode;
    uint8_t rs;
    uint8_t rt;
    uint8_t rd;
    uint8_t shamt;
    uint8_t funct;
    uint32_t immediate;
    uint32_t address;
} instruction;

void initializeReigsters(Reigsters *r) {
    for (int i = 0; i < 32; i++){
        r->Reg[i] = 0;
    }
    r->pc = 0;
    r->Reg[31] = 0xffffffff;//RA
    r->Reg[29] = 0x10000000;//SP
}
void RF_Init() {
    Register[sp] = 0x1000000;
    Register[ra] = 0xffffffff;
}

// read data from the registers
void RF_Read(uint8_t RdReg1, uint8_t RdReg2) {
    ReadData1 = Register[RdReg1];
    ReadData2 = Register[RdReg2];
}

void execute(uint32_t instruction) {
    uint32_t opcode = (instruction >> 26) & 0x3F;
    uint8_t rs = (instruction >> 21) & 0x1F;
    uint8_t rt = (instruction >> 16) & 0x1F;
    uint8_t rd = (instruction >> 11) & 0x1F;
    uint32_t shamt = (instruction >> 6) & 0x1F;
    uint32_t funct = instruction & 0x3F;
    uint32_t imm = instruction & 0xFFFF;
    uint32_t addr = instruction & 0x3FFFFFF;

    switch(opcode){
        case 0x00: //r type
        switch (funct){
            case 0x20: // ADD
                Reigsters[rd] = Reigsters[rs] + Reigsters[rt];
                break;
            case 0x22: // SUB
                Reigsters[rd] = Reigsters[rs] - Reigsters[rt];
                break;
            case 0x24: // AND
                Reigsters[rd] = Reigsters[rs] & Reigsters[rt];
                break;
            case 0x25: // OR
                Reigsters[rd] = Reigsters[rs] | Reigsters[rt];
                break;
            case 0x27: // NOR
                Reigsters[rd] = ~(Reigsters[rs] | Reigsters[rt]);
                break;
            case 0x2A: // SLT (Set on Less Than)
                if ((int32_t)Reigsters[rs] < (int32_t)Reigsters[rt])
                Reigsters[rd] = 1;
                else
                Reigsters[rd] = 0;
                break;
            case 0x00: // SLL (Shift Left Logical)
                Reigsters[rd] = Reigsters[rt] << shamt;
                break;
            case 0x02: // SRL (Shift Right Logical)
                Reigsters[rd] = Reigsters[rt] >> shamt;
                break;

            default:
                printf("Unsupported R-type instruction function: 0x%X\n", funct);
        }
    }
}

void decode(){  
}
unit32_t fetch(Reigsters *r) {
    uint32_t instruction = memory[r->pc / 4]; // Fetch the instruction from memory
    r->pc += 4; // Increment the program counter
    return instruction;
}