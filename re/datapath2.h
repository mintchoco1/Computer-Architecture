// datapath.h
#ifndef DATAPATH_H
#define DATAPATH_H

#include <stdint.h>

#define Mem_size 0x1000000

typedef struct {
    uint32_t reg[32];
    uint32_t pc;
} Regs;

typedef struct {
    uint32_t mips_inst;
    uint8_t opcode;
    uint8_t rs;
    uint8_t rt;
    uint8_t rd;
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

extern uint8_t mem[Mem_size];
extern uint64_t hi, lo;
extern int inst_cnt, r_cnt, i_cnt, j_cnt, ma_cnt, b_cnt;

void init_regs(Regs *r);
uint32_t fetch(Regs *r, uint8_t *mem);
void decode(uint32_t inst, uint32_t *id_ex, Control *control);
uint32_t alu_control(uint8_t ALUOp, uint8_t funct);
uint32_t execute(Regs *r, uint32_t *id_ex, Control *control);
uint32_t mem_access(Regs *r, uint32_t *id_ex, Control control, uint32_t alu_result);
void write_back(Regs *r, uint32_t *id_ex, Control control, uint32_t mem_result, uint32_t alu_result);
void datapath(Regs *r, uint8_t *mem);

#endif