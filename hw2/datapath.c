#include "datapath.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

uint8_t mem[Mem_size];
Control_Signal control;
uint32_t instruction_count = 0;
uint64_t hi = 0, lo = 0;
uint32_t R_count = 0, I_count = 0, J_count = 0;
uint32_t mem_access_count = 0;
uint32_t branch_taken_count = 0;

void init_Registers(Registers *r) {
    for (int i = 0; i < 32; i++){
        r->Reg[i] = 0;
    }
    r->pc = 0;
    r->Reg[31] = 0xffffffff;//RA
    r->Reg[29] = 0x10000000;//SP
}

void init_control_signals(Control_Signal *control) {
    control->RegDest = 0;
    control->AluSrc = 0;
    control->MemToReg = 0;
    control->RegWrite = 0;
    control->MemRead = 0;
    control->MemWrite = 0;
    control->Jump = 0;
    control->JumpReg = 0;
    control->JumpLink = 0;
    control->ALUOP = 0;
    control->Branch = 0; 
}

int32_t sign_extend(uint16_t imm) {
    return (int16_t)imm;
}

void set_control_signals(Instruction *inst) {
    memset(&control, 0, sizeof(Control_Signal));

    if (inst->opcode == 0x00) {
        // R-type
        control.RegDest = 1;
        control.RegWrite = 1;
        control.ALUOP = 0b10;

        switch (inst->funct) {
            case 0x20: case 0x21: // add, addu
            case 0x22: case 0x23: // sub, subu
            case 0x24: case 0x25: // and, or
            case 0x26:            // xor
            case 0x27:            // nor
            case 0x2A: case 0x2B: // slt, sltu
            case 0x00: case 0x02: case 0x03: // sll, srl, sra
                // ALU 연산 - 기본 RegWrite 유지
                break;

            case 0x08: // jr
                control.JumpReg = 1;
                control.RegWrite = 0;
                break;

            case 0x09: // jalr
                control.JumpReg = 1;
                control.JumpLink = 1;
                // RegWrite 유지 (ra 저장)
                break;

            case 0x10: // mfhi
            case 0x12: // mflo
                // hi/lo -> 레지스터로 이동, RegWrite 유지
                break;

            case 0x11: // mthi
            case 0x13: // mtlo
                control.RegWrite = 0;
                break;

            case 0x18: case 0x19: // mult, multu
            case 0x1A: case 0x1B: // div, divu
                control.RegWrite = 0;
                break;

            default:
                control.RegWrite = 0;
                break;
        }

    } else if (inst->opcode == 0x02 || inst->opcode == 0x03) {
        // J-type
        control.Jump = 1;
        if (inst->opcode == 0x03) {
            control.JumpLink = 1;
            control.RegWrite = 1; // jal은 $ra 저장
        }

    } else {
        // I-type
        control.RegDest = 0;  // 목적지는 rt
        control.AluSrc = 1;   // immediate 사용

        switch (inst->opcode) {
            // 산술/논리 immediate 연산
            case 0x08: case 0x09: // addi, addiu
            case 0x0C: case 0x0D: // andi, ori
            case 0x0A: case 0x0B: // slti, sltiu
            case 0x0E:            // xori
            case 0x0F:            // lui
                control.RegWrite = 1;
                control.ALUOP = 0b00;
                break;

            // Load
            case 0x23: case 0x24: case 0x25: case 0x30: // lw, lbu, lhu, ll
                control.MemRead = 1;
                control.MemToReg = 1;
                control.RegWrite = 1;
                control.ALUOP = 0b00;
                break;

            // Store
            case 0x28: case 0x29: case 0x2B: // sb, sh, sw
                control.MemWrite = 1;
                control.ALUOP = 0b00;
                break;
            case 0x38: // sc
                control.MemWrite = 1;
                control.RegWrite = 1;
                control.ALUOP = 0b00;
                break;

            // Branch
            case 0x04: case 0x05: // beq, bne
            case 0x06: case 0x07: // blez, bgtz
            case 0x01:            // bltz, bgez
                control.Branch = 1;
                control.ALUOP = 0b01;
                break;

            default:
                // 정의되지 않은 I-type 명령어 보호
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
            case 0x26: 
                return 3; // XOR
            case 0x03: 
                return 10; // SRA
            case 0x18: 
                return 13; // MULT
            case 0x19: 
                return 14; // MULTU
            case 0x1A: 
                return 15; // DIV
            case 0x1B: 
                return 16; // DIVU
            case 0x10: 
                return 17; // MFHI
            case 0x12: 
                return 18; // MFLO
            case 0x11: 
                return 19; // MTHI
            case 0x13: 
                return 20; // MTLO
            default: 
                return 18;   // Undefined
        }
    }
    return 18; // Fallback
}

Instruction fetch(Registers *r, uint8_t *mem, Instruction *inst) {
    uint32_t pc_index = r->pc - 0x00400000; // 주소 변환 (예시 베이스 주소)
    if (pc_index + 4 > Mem_size) {
        printf("Error: PC out of bounds (0x%08x)\n", r->pc);
        exit(1);
    }
    inst->mips_inst = 0;
    for (int i = 0; i < 4; i++) {
        inst->mips_inst = (inst->mips_inst << 8) | (mem[pc_index + i] & 0xFF);
    }
    r->pc += 4;
    return *inst;
}

Instruction decode(Instruction *inst) {
    inst->opcode = (inst->mips_inst >> 26) & 0x3F;

    if (inst->opcode == 0x00) { // R-type
        inst->rs = (inst->mips_inst >> 21) & 0x1F;
        inst->rt = (inst->mips_inst >> 16) & 0x1F;
        inst->rd = (inst->mips_inst >> 11) & 0x1F;
        inst->shamt = (inst->mips_inst >> 6) & 0x1F;
        inst->funct = inst->mips_inst & 0x3F;
        //inst.type = R_TYPE;
    } else if (inst->opcode == 0x02 || inst->opcode == 0x03) { // J-type
        inst->address = inst->mips_inst & 0x3FFFFFF;
        //inst.type = J_TYPE;
    } else { // I-type
        inst->rs = (inst->mips_inst >> 21) & 0x1F;
        inst->rt = (inst->mips_inst >> 16) & 0x1F;
        inst->immediate = inst->mips_inst & 0xFFFF;
        //inst.type = I_TYPE;
    }
    return *inst;
}

void execute(Registers *r, Instruction *inst, uint32_t *alu_result) {
    uint32_t operand1 = r->Reg[inst->rs];
    uint32_t operand2 = control.AluSrc ? (uint32_t)sign_extend(inst->immediate) : r->Reg[inst->rt];

    uint8_t signal = alu_control(control.ALUOP, inst->funct);
    switch (signal) {
        case 0b0011: // XOR
            *alu_result = operand1 ^ operand2;
            break;
        case 0b1010: // SRA
            *alu_result = (int32_t)operand2 >> inst->shamt;
            break;
        case 0b1101: // MULT
            {
                int64_t result = (int64_t)((int32_t)operand1) * (int64_t)((int32_t)operand2);
                lo = result & 0xFFFFFFFF;
                hi = (result >> 32) & 0xFFFFFFFF;
                return;
            }
        case 14: // MULTU
            {
                uint64_t result = (uint64_t)operand1 * (uint64_t)operand2;
                lo = result & 0xFFFFFFFF;
                hi = (result >> 32) & 0xFFFFFFFF;
                return;
            }
        case 15: // DIV
            if ((int32_t)operand2 != 0) {
                lo = (int32_t)operand1 / (int32_t)operand2;
                hi = (int32_t)operand1 % (int32_t)operand2;
            }
            return;
        case 16: // DIVU
            if (operand2 != 0) {
                lo = operand1 / operand2;
                hi = operand1 % operand2;
            }
            return;
        case 17: // MFHI
            *alu_result = hi;
            break;
        case 18: // MFLO
            *alu_result = lo;
            break;
        case 19: // MTHI
            hi = operand1;
            return;
        case 20: // MTLO
            lo = operand1;
            return;
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
        
        switch (inst->opcode) {
            case 0x04: 
                taken = (*alu_result == 0); //beq
                break;
            case 0x05: 
                taken = (*alu_result != 0); // bne
                break;
            case 0x06: 
                taken = ((int32_t)operand1 <= 0); // / blez
                break;
            case 0x07: 
                taken = ((int32_t)operand1 > 0); // bgtz
                break;
            case 0x01:
                if (inst->rt == 0x00) 
                    taken = ((int32_t)operand1 < 0);  // bltz
                else if (inst->rt == 0x01) 
                    taken = ((int32_t)operand1 >= 0); // bgez
                break;
        }
        if (taken){
            branch_taken_count++; //실제 분기 발생 시 카운트 
            r->pc = r->pc + 4 + ((int16_t)inst->immediate << 2);
        }
    } else if (control.Jump) {
        if (control.JumpLink) 
            r->Reg[31] = r->pc;       
            
        r->pc = (r->pc & 0xf0000000) | (inst->address << 2);
    
    } else if (control.JumpReg) {
        if (control.JumpLink) 
            r->Reg[31] = r->pc;
            
        r->pc = r->Reg[inst->rs];
    }
}

uint32_t memory_access(Registers *r, Instruction inst, uint32_t alu_result) {
    uint32_t mem_index = alu_result - 0x80000000; // 예시 데이터 세그먼트 베이스 주소
    if (mem_index + 4 >= Mem_size) {
        printf("Memory access out of bounds: 0x%08x\n", alu_result);
        exit(1);
    }
    if (control.MemRead) {
        mem_access_count++; //메모리 읽기 카운트 증가
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

void run(Registers *r, uint8_t *mem) {
    Instruction inst;
    while (r->pc != 0xffffffff) {
        uint32_t old_regs[32];
        memcpy(old_regs, r->Reg, sizeof(old_regs));
        uint32_t old_pc = r->pc;

        fetch(r, mem, &inst);
        if (inst.mips_inst == 0) {
            printf("NOP\n\n");
            r->pc += 4;  
            continue;
        }

        instruction_count++;
        decode(&inst);
        set_control_signals(&inst);

        uint32_t alu_result = 0;
        execute(r, &inst, &alu_result);

        uint32_t mem_result = memory_access(r, inst, alu_result);
        write_back(r, inst, mem_result);

        print_diff(r, old_regs, old_pc);
        printf("--------------------------\n");

        if (inst.opcode == 0x00) R_count++;
        else if (inst.opcode == 0x02 || inst.opcode == 0x03) J_count++;
        else I_count++;
    }
}

int32_t sign_extend(uint16_t imm) {
    return (int16_t)imm;
}