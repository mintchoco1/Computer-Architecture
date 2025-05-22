#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define MEMORY_SIZE 0x10000000

extern uint8_t memory[MEMORY_SIZE];

typedef struct {
    uint32_t regsp[32];
    uint32_t pc;
} Registers;

typedef struct {
    int regdst;
    int regwrite;
    int alusrc;
    int memtoreg;
    int memwrite;
    int memread;
    int branch;
    int jump;
    int aluop;
} Control_Signals;

typedef struct {
    uint32_t opcode;
    uint32_t rs;
    uint32_t rt;
    uint32_t rd;
    uint32_t shamt;
    uint32_t funct;
    uint32_t immediate;
    uint32_t rs_value;
    uint32_t rt_value;
    uint32_t inst_type; //r, i, j, jr, branch, jalr
    uint32_t jump_target; //jump target
    uint32_t pc_plus_4; //pc + 4
} Instruction;

//==========Latches==========//
typedef struct {
    uint32_t instruction;
    uint32_t pc;
    bool valid; //포워딩, stall, flush, nop 처리, stage 스킵 처리
} IF_ID_Latch; //fetch된 명령어와 pc 저장

typedef struct {
    bool valid;
    uint32_t pc;
    Instruction instruction;
    Control_Signals control_signals;
    uint32_t rs_value;
    uint32_t rt_value;
    uint32_t write_reg;//목적지 레지스터
} ID_EX_Latch; //decode된 명령어와 pc 저장

typedef struct {
    bool valid;
    uint32_t pc;
    Instruction instruction;
    Control_Signals control_signals;
    uint32_t alu_result;
    uint32_t rt_value;
    uint32_t write_reg;
} EX_MEM_Latch; //execute된 명령어와 pc 저장

typedef struct {
    bool valid;
    uint32_t pc;
    Instruction instruction;
    Control_Signals control_signals;
    uint32_t alu_result;
    uint32_t rt_value;
    uint32_t write_reg;
} MEM_WB_Latch; //memory access된 명령어와 pc 저장

extern IF_ID_Latch if_id_latch;
extern ID_EX_Latch id_ex_latch;
extern EX_MEM_Latch ex_mem_latch;
extern MEM_WB_Latch mem_wb_latch;
extern Registers registers;

typedef struct {
    int forwarding_a;
    int forwarding_b;
} ForwardingUnit;

extern void decode_rtype(uint32_t, Instruction*);
extern void decode_itype(uint32_t, Instruction*);
extern void decode_jtype(uint32_t, Instruction*);
extern void setup_control_signals(Instruction*, Control_Signals*);