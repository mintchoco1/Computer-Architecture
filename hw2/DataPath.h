#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

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
    uint8_t ALUOP;
    int Branch; 
} Control_Signal;   

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

typedef enum {
    ALU_ADD,
    ALU_SUB,
    ALU_AND,
    ALU_OR,
    ALU_NOR,
    ALU_SLT,
    ALU_SLTU,
    ALU_SLL,
    ALU_SRL,
    ALU_LUI,
    ALU_INVALID
} ALUControlSignal;

void initializeReigsters(Reigsters *r) {
    for (int i = 0; i < 32; i++){
        r->Reg[i] = 0;
    }
    r->pc = 0;
    r->Reg[31] = 0xffffffff;//RA
    r->Reg[29] = 0x10000000;//SP
}

// read data from the registers
void RF_Read(uint8_t RdReg1, uint8_t RdReg2) {
    ReadData1 = Register[RdReg1];
    ReadData2 = Register[RdReg2];
}

void set_control_signals(instruction inst) {
    memset(&control, 0, sizeof(Control_Signal)); // 모든 신호 기본값 0

    // R-type 명령어 (opcode = 0)
    if (inst.opcode == 0x00) {
        switch (inst.funct) {
            case 0x20: // ADD
                control.RegDst = 1;
            case 0x21: // ADDU
            case 0x24: // AND
            case 0x25: // OR
            case 0x27: // NOR
            case 0x2A: // SLT
            case 0x2B: // SLTU
            case 0x00: // SLL
            case 0x02: // SRL
            case 0x22: // SUB
            case 0x23: // SUBU
                control.RegDst = 1;
                control.ALUSrc = (inst.funct == 0x00 || inst.funct == 0x02); // shift 연산은 shamt 사용
                control.RegWrite = 1;
                control.ALUOp = inst.funct;
                break;
            case 0x08: // JR
                control.JumpReg = 1;
                break;
            case 0x09: // JALR
                control.RegDst = 1;
                control.RegWrite = 1;
                control.JumpReg = 1;
                control.JumpLink = 1;
                control.ALUOp = 0x09;
                break;
        }
    }

    // I-type 명령어
    else {
        switch (inst.opcode) {
            case 0x08: // ADDI
                control.ALUSrc = 1;
                control.RegWrite = 1;
                control.ALUOp = 0x20;
                break;
            case 0x09: // ADDIU
                control.ALUSrc = 1;
                control.RegWrite = 1;
                control.ALUOp = 0x21;
                break;
            case 0x0C: // ANDI
                control.ALUSrc = 1;
                control.RegWrite = 1;
                control.ALUOp = 0x24;
                break;
            case 0x0D: // ORI
                control.ALUSrc = 1;
                control.RegWrite = 1;
                control.ALUOp = 0x25;
                break;
            case 0x0A: // SLTI
                control.ALUSrc = 1;
                control.RegWrite = 1;
                control.ALUOp = 0x2A;
                break;
            case 0x0B: // SLTIU
                control.ALUSrc = 1;
                control.RegWrite = 1;
                control.ALUOp = 0x2B;
                break;
            case 0x0F: // LUI
                control.ALUSrc = 1;
                control.RegWrite = 1;
                control.ALUOp = 0x0F;
                break;
            case 0x04: // BEQ
                control.Branch = 1;
                control.ALUOp = 0x22;
                break;
            case 0x05: // BNE
                control.Branch = 1;
                control.ALUOp = 0x23; // 분기를 위한 sub
                break;
            case 0x23: // LW
                control.ALUSrc = 1;
                control.MemToReg = 1;
                control.RegWrite = 1;
                control.MemRead = 1;
                control.ALUOp = 0x20;
                break;
            case 0x24: // LBU
            case 0x25: // LHU
                control.ALUSrc = 1;
                control.MemToReg = 1;
                control.RegWrite = 1;
                control.MemRead = 1;
                control.ALUOp = 0x20;
                break;
            case 0x30: // LL
                control.ALUSrc = 1;
                control.MemToReg = 1;
                control.RegWrite = 1;
                control.MemRead = 1;
                control.ALUOp = 0x20;
                break;
            case 0x28: // SB
            case 0x29: // SH
            case 0x2B: // SW
                control.ALUSrc = 1;
                control.MemWrite = 1;
                control.ALUOp = 0x20;
                break;
            case 0x38: // SC
                control.ALUSrc = 1;
                control.RegWrite = 1;
                control.MemWrite = 1;
                control.ALUOp = 0x20;
                break;
            case 0x02: // J
                control.Jump = 1;
                break;
            case 0x03: // JAL
                control.Jump = 1;
                control.JumpLink = 1;
                control.RegWrite = 1;
                break;
        }
    }
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
}

uint32_t memory_access(Instruction inst, uint32_t alu_result) {
    if (control.MemRead) {
        return *((uint32_t*)&MEMORY[alu_result]);
    } else if (control.MemWrite) {
        *((uint32_t*)&MEMORY[alu_result]) = REGS[inst.rt];
    }
    return alu_result;
}

instruction decode(instruction inst) {
    inst.opcode = (inst.raw >> 26) & 0x3F;
    if (inst.opcode == 0x00) { // R-type
        inst.rs = (inst.raw >> 21) & 0x1F;
        inst.rt = (inst.raw >> 16) & 0x1F;
        inst.rd = (inst.raw >> 11) & 0x1F;
        inst.shamt = (inst.raw >> 6) & 0x1F;
        inst.funct = inst.raw & 0x3F;
        inst.type = R_TYPE;
    } else if (inst.opcode == 0x02 || inst.opcode == 0x03) { // J-type
        inst.address = inst.raw & 0x3FFFFFF;
        inst.type = J_TYPE;
    } else { // I-type
        inst.rs = (inst.raw >> 21) & 0x1F;
        inst.rt = (inst.raw >> 16) & 0x1F;
        inst.immediate = inst.raw & 0xFFFF;
        inst.type = I_TYPE;
    }
    return inst;
}

void write_back(Instruction inst, uint32_t result) {
    if (!control.RegWrite) return;

    uint8_t dest = control.RegDst ? inst.rd : inst.rt;
    if (control.JumpLink) dest = 31;

    if (control.MemToReg) {
        REGS[dest] = result;
    } else if (control.JumpLink) {
        REGS[31] = PC;
    } else {
        REGS[dest] = result;
    }
}