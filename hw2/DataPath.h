#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define Mem_size 0x10000

uint32_t mem[Mem_size];

typedef struct{
    uint32_t Reg[32]; // 32 Reigsters
    int pc; // program counter
} Registers;

typedef struct {
    int RegDest;
    int AluSrc;
    int MemToReg;
    int RegWrite;
    int MemRead;
    int MemWrite;
    int Jump;
    int JumpReg;
    int JumpLink;
    uint8_t ALUOP;
    int Branch; 
} Control_Signal;   

typedef struct{
    uint8_t opcode;
    uint8_t rs;
    uint8_t rt;
    uint8_t rd;
    uint8_t shamt;
    uint8_t funct;
    uint16_t immediate;
    uint32_t address;
    uint32_t raw; // 전체 명령어
} Instruction;

Control_Signal control;
uint32_t instruction_count = 0;
uint32_t R_count = 0, I_count = 0, J_count = 0;

void init_Registers(Registers *r) {
    for (int i = 0; i < 32; i++){
        r->Reg[i] = 0;
    }
    r->pc = 0;
    r->Reg[31] = 0xffffffff;//RA
    r->Reg[29] = 0x10000000;//SP
}

void set_control_signals(Instruction *inst) {
    memset(&control, 0, sizeof(Control_Signal)); // 모든 신호 기본값 0

    // R-type 명령어 (opcode = 0)
    if (inst->opcode == 0x00) {
        control.RegDest = 1; // R-type 명령어는 rd에 결과 저장
        control.RegWrite = 1; // 레지스터에 쓰기
        control.ALUOP = inst->funct; // ALU 연산 코드 설정

        switch (inst->funct) {
            case 0x20: // ADD
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
                control.RegDest = 1;
                control.AluSrc = (inst->funct == 0x00 || inst->funct == 0x02); // shift 연산은 shamt 사용
                control.RegWrite = 1;
                control.ALUOP = inst->funct;
                break;
            case 0x08: // JR
                control.JumpReg = 1;
                control.RegWrite = 0; // 레지스터에 쓰지 않음
                break;
            case 0x09: // JALR
                control.RegDest = 1;
                control.RegWrite = 1;
                control.JumpReg = 1;
                control.JumpLink = 1;
                control.ALUOP = 0x09;
                break;
        }
    }

    // I-type 명령어
    else {
        control.RegDest = 0; // I-type 명령어는 rt에 결과 저장
        control.AluSrc = 1; // ALU의 두 번째 피연산자로 immediate 사용

        switch (inst->opcode) {
            case 0x08: // ADDI
                control.RegWrite = 1;
                control.ALUOP = 0x20;
                break;
            case 0x09: // ADDIU
                control.AluSrc = 1;
                control.RegWrite = 1;
                control.ALUOP = 0x21;
                break;
            case 0x0C: // ANDI
                control.AluSrc = 1;
                control.RegWrite = 1;
                control.ALUOP = 0x24;
                break;
            case 0x0D: // ORI
                control.AluSrc = 1;
                control.RegWrite = 1;
                control.ALUOP = 0x25;
                break;
            case 0x0A: // SLTI
                control.AluSrc = 1;
                control.RegWrite = 1;
                control.ALUOP = 0x2A;
                break;
            case 0x0B: // SLTIU
                control.AluSrc = 1;
                control.RegWrite = 1;
                control.ALUOP = 0x2B;
                break;
            case 0x0F: // LUI
                control.AluSrc = 1;
                control.RegWrite = 1;
                control.ALUOP = 0x0F;
                break;
            case 0x04: // BEQ
                control.Branch = 1;
                control.ALUOP = 0x22;
                break;
            case 0x05: // BNE
                control.Branch = 1;
                control.ALUOP = 0x23; // 분기를 위한 sub
                break;
            case 0x23: // LW
                control.MemToReg = 1;
                control.RegWrite = 1;
                control.MemRead = 1;
                control.ALUOP = 0x20;
                break;
            case 0x24: // LBU
            case 0x25: // LHU
                control.MemToReg = 1;
                control.RegWrite = 1;
                control.MemRead = 1;
                control.ALUOP = 0x20;
                break;
            case 0x30: // LL
                control.MemToReg = 1;
                control.RegWrite = 1;
                control.MemRead = 1;
                control.ALUOP = 0x20;
                break;
            case 0x28: // SB
            case 0x29: // SH
            case 0x2B: // SW
                control.AluSrc = 1;
                control.MemWrite = 1;
                control.ALUOP = 0x20;
                break;
            case 0x38: // SC
                control.AluSrc = 1;
                control.RegWrite = 1;
                control.MemWrite = 1;
                control.ALUOP = 0x20;
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

uint32_t alu_control(uint8_t alu_op, uint32_t a, uint32_t b, uint8_t shamt) {
    switch (alu_op) {
        case 0x20: 
            return a + b;            // ADD, ADDI, ADDU, ADDIU, LW, etc.
        case 0x21: 
            return a + b;            // ADDU
        case 0x22: 
            return a - b;            // SUB
        case 0x23: 
            return a - b;            // BNE (treated as SUB)
        case 0x24: 
            return a & b;            // AND, ANDI
        case 0x25: 
            return a | b;            // OR, ORI
        case 0x27: 
            return ~(a | b);         // NOR
        case 0x2A: 
            return ((int32_t)a < (int32_t)b); // SLT, SLTI
        case 0x2B: 
            return (a < b);          // SLTU, SLTIU
        case 0x00: 
            return b << shamt;       // SLL
        case 0x02: 
            return b >> shamt;       // SRL
        case 0x0F: 
            return b << 16;          // LUI
        default: 
            return 0;
    }
}

Instruction fetch(Registers *r, uint8_t *mem, Instruction *inst) {
    if (r->pc + 4 > Mem_size) {
        printf("Error: PC out of bounds\n");
        exit(1);
    }
    inst->raw = 0;
    for (int i = 0; i < 4; i++) {
        inst->raw = (inst->raw << 8) | (mem[r->pc + i] & 0xFF);
    }
    r->pc += 4;
    return *inst;
}

Instruction decode(Instruction *inst) {
    inst->opcode = (inst->raw >> 26) & 0x3F;
    if (inst->opcode == 0x00) { // R-type
        inst->rs = (inst->raw >> 21) & 0x1F;
        inst->rt = (inst->raw >> 16) & 0x1F;
        inst->rd = (inst->raw >> 11) & 0x1F;
        inst->shamt = (inst->raw >> 6) & 0x1F;
        inst->funct = inst->raw & 0x3F;
        //inst.type = R_TYPE;
    } else if (inst->opcode == 0x02 || inst->opcode == 0x03) { // J-type
        inst->address = inst->raw & 0x3FFFFFF;
        //inst.type = J_TYPE;
    } else { // I-type
        inst->rs = (inst->raw >> 21) & 0x1F;
        inst->rt = (inst->raw >> 16) & 0x1F;
        inst->immediate = inst->raw & 0xFFFF;
        //inst.type = I_TYPE;
    }
    return *inst;
}

uint32_t memory_access(Registers *r, Instruction inst, uint32_t alu_result) {
    if (control.MemRead) {
        return *((uint32_t*)&mem[alu_result]);
    } else if (control.MemWrite) {
        *((uint32_t*)&mem[alu_result]) = r->Reg[inst.rt];
    }
    return alu_result;
}

void write_back(Registers *r, Instruction inst, uint32_t result) {
    if (!control.RegWrite) 
        return;
    uint8_t dest = control.RegDest ? inst.rd : inst.rt;
    if (control.JumpLink) 
        dest = 31;
    if (control.MemToReg) {
        r->Reg[dest] = result;
    } else if (control.JumpLink) {
        r->Reg[31] = r->pc;
    } else {
        r->Reg[dest] = result;
    }
}

void print_diff(Registers *r, uint32_t old_regs[], uint32_t old_pc) {
    for (int i = 0; i < 32; i++) {
        if (r->Reg[i] != old_regs[i]) {
            printf("  $%d changed from 0x%08x to 0x%08x\n", i, old_regs[i], r->Reg[i]);
        }
    }
    if (r->pc != old_pc) {
        printf("  PC changed from 0x%08x to 0x%08x\n", old_pc, r->pc);
    }
}