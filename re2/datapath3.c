#include "datapath3.h"
#include <stdio.h>
#include <string.h>

int inst_cnt = 0, r_cnt = 0, i_cnt = 0, j_cnt = 0, mem_cnt = 0, branch_cnt = 0;
extern uint8_t mem[MEM_SIZE];

void init_regs(uint32_t *regs) {
    for (int i = 0; i < 32; i++) regs[i] = 0;
    regs[29] = 0x1000000; // Stack pointer
    regs[31] = 0xFFFFFFFF; // Return address
}

Instruction fetch(uint32_t pc) {
    Instruction inst = {0};
    inst.inst = (mem[pc] << 24) | (mem[pc + 1] << 16) | (mem[pc + 2] << 8) | mem[pc + 3];
    return inst;
}

void decode(Instruction *inst, Control *control) {
    inst->opcode = (inst->inst >> 26) & 0x3F;
    inst->rs = (inst->inst >> 21) & 0x1F;
    inst->rt = (inst->inst >> 16) & 0x1F;
    inst->rd = (inst->inst >> 11) & 0x1F;
    inst->shamt = (inst->inst >> 6) & 0x1F;
    inst->funct = inst->inst & 0x3F;
    inst->immediate = inst->inst & 0xFFFF;
    inst->address = inst->inst & 0x03FFFFFF;

    control_signal(inst->opcode, inst->funct, control);

    if (inst->opcode == 0x00) r_cnt++;
    else if (inst->opcode == 0x02 || inst->opcode == 0x03) j_cnt++;
    else i_cnt++;
}

uint32_t execute(Control control, Instruction inst, uint32_t *regs) {
    printf("\t[Execute]\n");

    uint32_t operand1 = regs[inst.rs];
    uint32_t operand2 = control.ALUSrc ? ((inst.opcode == 0x0C || inst.opcode == 0x0D || inst.opcode == 0x0E) ? (uint32_t)(inst.immediate) : (int32_t)(int16_t)(inst.immediate)) : regs[inst.rt];

    uint8_t alu_sig = alu_control(control.ALUOp, inst.funct);

    uint32_t result;
    switch (alu_sig) {
        case 0b0000: result = operand1 & operand2; break;
        case 0b0001: result = operand1 | operand2; break;
        case 0b0010: result = operand1 + operand2; break;
        case 0b0110: result = operand1 - operand2; break;
        case 0b0111: result = ((int32_t)operand1 < (int32_t)operand2); break;
        case 0b1100: result = ~(operand1 | operand2); break;
        case 0b1000: result = operand2 << inst.shamt; break;
        case 0b1001: result = operand2 >> inst.shamt; break;
        default: result = 0; break;
    }

    printf("\t[Execute] ALU Result = 0x%08X\n", result);
    return result;
}

uint32_t mem_access(Control control, Instruction inst, uint32_t alu_result, uint32_t *regs) {
    printf("\t[Memory Access]\n");
    if (control.MemRead) {
        uint32_t val = 0;
        for (int i = 0; i < 4; i++) {
            val |= mem[alu_result + i] << (8 * (3 - i));
        }
        printf("\t[Memory Access] Loaded Value = 0x%08X\n", val);
        mem_cnt++;
        return val;
    } else if (control.MemWrite) {
        for (int i = 0; i < 4; i++) {
            mem[alu_result + i] = (regs[inst.rt] >> (8 * (3 - i))) & 0xFF;
        }
        printf("\t[Memory Access] Stored Value = 0x%08X\n", regs[inst.rt]);
        mem_cnt++;
    }
    return alu_result;
}

void write_back(Control control, Instruction inst, uint32_t alu_result, uint32_t mem_result, uint32_t *regs) {
    printf("\t[Write Back]\n");
    if (!control.RegWrite) return;
    uint32_t write_data = control.MemToReg ? mem_result : alu_result;
    uint8_t dest = control.RegDst ? inst.rd : inst.rt;
    regs[dest] = write_data;
    printf("\t[Write Back] Reg[%d] <= 0x%08X\n", dest, write_data);
}

void datapath(uint32_t *regs) {
    uint32_t pc = 0;
    while (1) {
        if (pc + 3 >= MEM_SIZE) {
            printf("\n[Error] PC out of memory range. Terminating.\n");
            break;
        }

        printf("Cycle %d:\n", inst_cnt);

        Instruction inst = fetch(pc);
        if (inst.inst == 0) {
            printf("\t[Instruction Fetch] NOP\n");
            break;
        }

        printf("\t[Instruction Fetch] 0x%08X (PC=0x%08X)\n", inst.inst, pc);

        Control control;
        decode(&inst, &control);

        uint32_t alu_result = execute(control, inst, regs);
        uint32_t mem_result = mem_access(control, inst, alu_result, regs);
        write_back(control, inst, alu_result, mem_result, regs);

        if (control.Jump) {
            if (inst.opcode == 0x03) regs[31] = pc + 4;
            pc = (pc & 0xF0000000) | (inst.address << 2);
        } else if (control.JumpReg) {
            pc = regs[inst.rs];
        } else if (control.Branch) {
            uint32_t zero = (alu_result == 0);
            if ((inst.opcode == 0x04 && zero) || (inst.opcode == 0x05 && !zero)) {
                branch_cnt++;
                pc += 4 + ((int32_t)(int16_t)(inst.immediate) << 2);
            } else {
                pc += 4;
            }
        } else {
            pc += 4;
        }

        inst_cnt++;
    }
}
