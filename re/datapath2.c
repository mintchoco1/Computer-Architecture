// datapath.c
#include "datapath2.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

uint8_t mem[Mem_size];
uint64_t hi = 0, lo = 0;
int inst_cnt = 0, r_cnt = 0, i_cnt = 0, j_cnt = 0, ma_cnt = 0, b_cnt = 0;

void init_regs(Regs *r) {
    for (int i = 0; i < 32; i++) r->reg[i] = 0;
    r->reg[29] = 0x1000000;
    r->reg[31] = 0xFFFFFFFF;
    r->pc = 0;
}

void control_signal(uint32_t opcode, uint32_t funct, Control *control) {
    memset(control, 0, sizeof(Control));

    if (opcode == 0x00) { // R-type
        control->RegDst = 1;
        control->RegWrite = 1;
        control->ALUOp = 0b10;
        if (funct == 0x08 || funct == 0x09) control->JumpReg = 1; // jr, jalr
    } else if (opcode == 0x02 || opcode == 0x03) { // J-type
        control->Jump = 1;
        if (opcode == 0x03) control->RegWrite = 1; // jal
    } else { // I-type
        control->ALUSrc = 1;
        switch (opcode) {
            case 0x08: case 0x09: // addi, addiu
                control->RegWrite = 1; control->ALUOp = 0b00; break;
            case 0x0C: case 0x0D: case 0x0E: // andi, ori, xori
                control->RegWrite = 1; control->ALUOp = 0b11; break;
            case 0x0A: case 0x0B: // slti, sltiu
                control->RegWrite = 1; control->ALUOp = 0b11; break;
            case 0x0F: // lui
                control->RegWrite = 1; break;
            case 0x23: // lw
                control->MemRead = 1; control->MemToReg = 1; control->RegWrite = 1; control->ALUOp = 0b00; break;
            case 0x2B: // sw
                control->MemWrite = 1; control->ALUOp = 0b00; break;
            case 0x04: case 0x05: // beq, bne
                control->Branch = 1; control->ALUOp = 0b01; break;
            default:
                control->RegWrite = 1; control->ALUOp = 0b00;
        }
    }
}

uint32_t fetch(Regs *r, uint8_t *mem) {
    printf("\t[Instruction Fetch] ");
    uint32_t inst = 0;
    for (int i = 0; i < 4; i++) {
        inst = (inst << 8) | mem[r->pc + i];
    }
    printf("0x%08X (PC=0x%08X)\n", inst, r->pc);
    r->pc += 4;
    return inst;
}

void decode(uint32_t inst, uint32_t *id_ex, Control *control) {
    printf("\t[Instruction Decode] ");
    id_ex[0] = (inst >> 26) & 0x3F;
    if (id_ex[0] == 0x00) {
        id_ex[1] = (inst >> 21) & 0x1F;
        id_ex[2] = (inst >> 16) & 0x1F;
        id_ex[3] = (inst >> 11) & 0x1F;
        id_ex[4] = (inst >> 6) & 0x1F;
        id_ex[5] = inst & 0x3F;
        control_signal(id_ex[0], id_ex[5], control);
        printf("R-type\n");
        r_cnt++;
    } else if (id_ex[0] == 0x02 || id_ex[0] == 0x03) {
        id_ex[1] = inst & 0x03FFFFFF;
        control_signal(id_ex[0], 0, control);
        printf("J-type\n");
        j_cnt++;
    } else {
        id_ex[1] = (inst >> 21) & 0x1F;
        id_ex[2] = (inst >> 16) & 0x1F;
        id_ex[3] = inst & 0xFFFF;
        control_signal(id_ex[0], 0, control);
        printf("I-type\n");
        i_cnt++;
    }
}

uint32_t alu_control(uint8_t ALUOp, uint8_t funct) {
    if (ALUOp == 0b00) return 0b0010;
    if (ALUOp == 0b01) return 0b0110;
    switch (funct) {
        case 0x20: return 0b0010;
        case 0x22: return 0b0110;
        case 0x24: return 0b0000;
        case 0x25: return 0b0001;
        case 0x2A: return 0b0111;
        default: return 0b0010;
    }
}

uint32_t execute(Regs *r, uint32_t *id_ex, Control *control) {
    printf("\t[Execute]\n");
    if (control->JumpReg) {
        r->pc = r->reg[id_ex[1]];
        if (id_ex[5] == 0x09) r->reg[31] = r->pc + 4; // jalr
        printf("\tJR or JALR executed\n");
        return 0;
    }

    uint32_t operand1 = r->reg[id_ex[1]];
    uint32_t operand2 = control->ALUSrc ? (int16_t)id_ex[3] : r->reg[id_ex[2]];
    uint8_t alu_sig = alu_control(control->ALUOp, id_ex[5]);

    switch (alu_sig) {
        case 0b0000: return operand1 & operand2;
        case 0b0001: return operand1 | operand2;
        case 0b0010: return operand1 + operand2;
        case 0b0110: return operand1 - operand2;
        case 0b0111: return ((int32_t)operand1 < (int32_t)operand2);
        default: return 0;
    }
}

uint32_t mem_access(Regs *r, uint32_t *id_ex, Control control, uint32_t alu_result) {
    printf("\t[Memory Access]\n");
    if (control.MemRead) {
        ma_cnt++;
        uint32_t val = 0;
        for (int i = 0; i < 4; i++) {
            val |= mem[alu_result + i] << (8 * i);
        }
        return val;
    } else if (control.MemWrite) {
        ma_cnt++;
        for (int i = 0; i < 4; i++) {
            mem[alu_result + i] = (r->reg[id_ex[2]] >> (8 * i)) & 0xFF;
        }
    }
    return alu_result;
}

void write_back(Regs *r, uint32_t *id_ex, Control control, uint32_t mem_result, uint32_t alu_result) {
    printf("\t[Write Back]\n");
    if (!control.RegWrite) return;
    uint32_t write_data = control.MemToReg ? mem_result : alu_result;
    uint8_t dest = control.RegDst ? id_ex[3] : id_ex[2];
    r->reg[dest] = write_data;
}

void datapath(Regs *r, uint8_t *mem) {
    while (r->pc != 0xFFFFFFFF) {
        printf("Cycle %d:\n", inst_cnt);
        uint32_t inst = fetch(r, mem);
        if (inst == 0) {
            printf("\tNOP\n\n");
            continue;
        }
        uint32_t id_ex[6] = {0};
        Control control;
        decode(inst, id_ex, &control);
        uint32_t alu_result = execute(r, id_ex, &control);
        uint32_t mem_result = mem_access(r, id_ex, control, alu_result);
        write_back(r, id_ex, control, mem_result, alu_result);
        printf("\n");
        inst_cnt++;
    }
}