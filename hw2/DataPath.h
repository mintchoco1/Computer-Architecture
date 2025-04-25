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
    memset(&control, 0, sizeof(Control_Signal));

    if (inst->opcode == 0x00) {
        // R-type
        control.RegDest = 1;
        control.RegWrite = 1;
        control.ALUOP = 0b10;

        switch (inst->funct) {
            case 0x20:// add
            case 0x21:// addu
            case 0x22:// sub
            case 0x23:// subu
            case 0x24:// and 
            case 0x25:// or
            case 0x27:// nor
            case 0x2A:// slt
            case 0x2B:// sltu
            case 0x00:// sll
            case 0x02://
                break; // 계산은 ALU에서 funct로 처리
            case 0x08: // jr
                control.JumpReg = 1;
                control.RegWrite = 0;
                break;
            case 0x09: // jalr
                control.JumpReg = 1;
                control.JumpLink = 1;
                break;
        }
    } else if (inst->opcode == 0x02 || inst->opcode == 0x03) {
        // J-type
        control.Jump = 1;
        if (inst->opcode == 0x03) {
            control.JumpLink = 1;
            control.RegWrite = 1;
        }
    } else {
        // I-type
        control.RegDest = 0;
        control.AluSrc = 1;

        switch (inst->opcode) {
            case 0x08: 
            case 0x09: 
            case 0x0C: 
            case 0x0D:
            case 0x0A: 
            case 0x0B: 
            case 0x0F:
                control.RegWrite = 1;
                control.ALUOP = 0b00;
                break;
            case 0x23: 
            case 0x24: 
            case 0x25: 
            case 0x30:
                control.MemRead = 1;
                control.MemToReg = 1;
                control.RegWrite = 1;
                control.ALUOP = 0b00;
                break;
            case 0x28: 
            case 0x29: 
            case 0x2B: 
            case 0x38:
                control.MemWrite = 1;
                control.RegWrite = (inst->opcode == 0x38);
                control.ALUOP = 0b00;
                break;
            case 0x04: 
            case 0x05:
                control.Branch = 1;
                control.ALUOP = 0b01;
                break;
        }
    }
}

uint8_t alu_control(uint8_t alu_op, uint8_t funct) {
    if (alu_op == 0b00) 
        return 0b0010; // ADD
    else if (alu_op == 0b01) 
        return 0b0110; // SUB
    else if (alu_op == 0b10) {
        switch (funct) {
            case 0x20:
                return 0b0010; // ADD
            case 0x21: 
                return 0b0010; // ADDU
            case 0x22:
                return 0b0110; // SUB 
            case 0x23:
                return 0b0110; // SUBU
            case 0x24: 
                return 0b0000; // AND
            case 0x25: 
                return 0b0001; // OR
            case 0x2A: 
                return 0b0111; // SLT
            case 0x27: 
                return 0b1100; // NOR
            case 0x00: 
                return 0b1000; // SLL
            case 0x02: 
                return 0b1001; // SRL
            default: return 0b1111;   // Undefined
        }
    }
    return 0b1111; // Fallback
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

void execute(Registers *r, Instruction *inst, uint32_t *alu_result) {
    uint32_t operand1 = r->Reg[inst->rs];
    uint32_t operand2 = control.AluSrc ? sign_extend(inst->immediate) : r->Reg[inst->rt];

    uint8_t signal = alu_control(control.ALUOP, inst->funct);
    switch (signal) {
        case 0b0000: //AND
            *alu_result = operand1 & operand2; 
            break;
        case 0b0001: //OR   
            *alu_result = operand1 | operand2; 
            break;
        case 0b0010: //ADD
            *alu_result = operand1 + operand2; 
            break;
        case 0b0110: 
            *alu_result = operand1 - operand2; 
            break;
        case 0b0111: 
            *alu_result = ((int32_t)operand1 < (int32_t)operand2); 
            break;
        case 0b1100: 
            *alu_result = ~(operand1 | operand2); 
            break;
        case 0b1000: 
            *alu_result = operand2 << inst->shamt; 
            break; // SLL
        case 0b1001: 
            *alu_result = operand2 >> inst->shamt; 
            break; // SRL
        default: 
            *alu_result = 0; 
            break;
    }

    if (control.Branch) {
        int taken = 0;
        if (inst->opcode == 0x04 && *alu_result == 0) taken = 1;
        else if (inst->opcode == 0x05 && *alu_result != 0) taken = 1;
        if (taken) r->pc = r->pc + ((int16_t)inst->immediate << 2);
    } else if (control.Jump) {
        if (control.JumpLink) r->Reg[31] = r->pc;
        r->pc = (r->pc & 0xF0000000) | (inst->address << 2);
    } else if (control.JumpReg) {
        if (control.JumpLink) r->Reg[31] = r->pc;
        r->pc = r->Reg[inst->rs];
    }
}

uint32_t memory_access(Registers *r, Instruction inst, uint32_t alu_result) {
    if (control.MemRead) {
        uint32_t val = 0;
        for (int i = 0; i < 4; i++) {
            val |= mem[alu_result + i] << (8 * i);
        }
        return val;
    } else if (control.MemWrite) {
        for (int i = 0; i < 4; i++) {
            mem[alu_result + i] = (r->Reg[inst.rt] >> (8 * i)) & 0xFF;
        }
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

int32_t sign_extend(uint16_t imm) {
    return (int16_t)imm;
}