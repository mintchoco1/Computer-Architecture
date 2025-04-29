#ifndef DATAPATH_H
#define DATAPATH_H

#include <stdint.h>

#define MEM_SIZE 0x2000000

typedef struct {
    uint32_t inst;
    uint8_t opcode;
    uint8_t rs, rt, rd;
    uint8_t shamt;
    uint8_t funct;
    uint16_t immediate;
    uint32_t address;
} Instruction;

typedef struct {
    int RegDst;
    int ALUSrc;
    int MemToReg;
    int RegWrite;
    int MemRead;
    int MemWrite;
    int Branch;
    int Jump;
    int JumpReg;
    uint8_t ALUOp;
} Control;

extern uint8_t mem[MEM_SIZE];
extern int inst_cnt, r_cnt, i_cnt, j_cnt, mem_cnt, branch_cnt;

void init_regs(uint32_t *regs);
Instruction fetch(uint32_t pc);
void decode(Instruction *inst, Control *control);
uint8_t alu_control(uint8_t ALUOp, uint8_t funct);
uint32_t execute(Control control, Instruction inst, uint32_t *regs);
uint32_t mem_access(Control control, Instruction inst, uint32_t alu_result, uint32_t *regs);
void write_back(Control control, Instruction inst, uint32_t alu_result, uint32_t mem_result, uint32_t *regs);
void datapath(uint32_t *regs);

#endif
