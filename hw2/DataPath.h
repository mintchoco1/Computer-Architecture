#ifndef DATAPATH_H
#define DATAPATH_H

#include <stdint.h>

#define Mem_size 0x10000

typedef struct {
    uint32_t Reg[32];
    int pc;
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

typedef struct {
    uint8_t opcode;
    uint8_t rs;
    uint8_t rt;
    uint8_t rd;
    uint8_t shamt;
    uint8_t funct;
    uint16_t immediate;
    uint32_t address;
    uint32_t mips_inst;
} Instruction;

// 전역변수 'extern'으로 선언
extern uint32_t mem[Mem_size];
extern Control_Signal control;
extern uint32_t instruction_count, hi, lo;
extern uint32_t R_count, I_count, J_count;
extern uint32_t mem_access_count;
extern uint32_t branch_taken_count;

// 함수 선언
void init_Registers(Registers *r);
Instruction fetch(Registers *r, uint8_t *mem, Instruction *inst);
Instruction decode(Instruction *inst);
void set_control_signals(Instruction *inst);
uint8_t alu_control(uint8_t alu_op, uint8_t funct);
void execute(Registers *r, Instruction *inst, uint32_t *alu_result);
uint32_t memory_access(Registers *r, Instruction inst, uint32_t alu_result);
void write_back(Registers *r, Instruction inst, uint32_t result);
void print_diff(Registers *r, uint32_t old_regs[], uint32_t old_pc);
int32_t sign_extend(uint16_t imm);

#endif