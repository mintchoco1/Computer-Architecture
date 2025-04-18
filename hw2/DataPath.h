#include <stdio.h>
#include <stdint.h>

#define Mem_size 2048

uint32_t memory[Mem_size];

typedef struct{
    uint32_t Reg[32]; // 32 registers
    int pc; // program counter
} Registers;

typedef struct {
    int RegDest;
    int AluSrc;
    int MemToReg;
    int RegWrite;
    int MemRead;
    int MemWrite;
} control_signal;   

void initializeRegisters(Registers *r) {
    for (int i = 0; i < 32; i++){
        r->Reg[i] = 0;
    }
    r->pc = 0;
    r->Reg[31] = 0xFFFFFFFF;//RA
    r->Reg[29] = 0x10000000;//SP
}

void execute(uint32_t instruction) {
    uint32_t opcode = (instruction >> 26) & 0x3F;
    uint32_t rs = (instruction >> 21) & 0x1F;
    uint32_t rt = (instruction >> 16) & 0x1F;
    uint32_t rd = (instruction >> 11) & 0x1F;
    uint32_t shamt = (instruction >> 6) & 0x1F;
    uint32_t funct = instruction & 0x3F;
    uint32_t imm = instruction & 0xFFFF;
    uint32_t addr = instruction & 0x3FFFFFF;

    switch(opcode){
        case 0x00: //r type
        switch (funct){
            case 0x20: // ADD
                Registers[rd] = Registers[rs] + Registers[rt];
                break;
            case 0x22: // SUB
                registers[rd] = registers[rs] - registers[rt];
                break;
            case 0x24: // AND
                registers[rd] = registers[rs] & registers[rt];
                break;
            case 0x25: // OR
                registers[rd] = registers[rs] | registers[rt];
                break;
            case 0x27: // NOR
                registers[rd] = ~(registers[rs] | registers[rt]);
                break;
            case 0x2A: // SLT (Set on Less Than)
                if ((int32_t)registers[rs] < (int32_t)registers[rt])
                registers[rd] = 1;
                else
                registers[rd] = 0;
                break;
            case 0x00: // SLL (Shift Left Logical)
                registers[rd] = registers[rt] << shamt;
                break;
            case 0x02: // SRL (Shift Right Logical)
                registers[rd] = registers[rt] >> shamt;
                break;

            default:
                printf("Unsupported R-type instruction function: 0x%X\n", funct);
        }
    }
}

void decode(){  
}
unit32_t fetch(Registers *r) {
    uint32_t instruction = memory[r->pc / 4]; // Fetch the instruction from memory
    r->pc += 4; // Increment the program counter
    return instruction;
}