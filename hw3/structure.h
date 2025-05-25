#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define MEMORY_SIZE 0x10000000

extern uint8_t memory[MEMORY_SIZE];

typedef struct {
    uint32_t regs[32];
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
    uint32_t inst_type; // r, i, j, jr, branch, jalr
    uint32_t jump_target; // jump target
    uint32_t pc_plus_4; // pc + 4
} Instruction;

// Latches
typedef struct {
    uint32_t instruction;
    uint32_t pc;
    bool valid;
} IF_ID_Latch;

typedef struct {
    bool valid;
    uint32_t pc;
    Instruction instruction;
    Control_Signals control_signals;
    uint32_t rs_value;
    uint32_t rt_value;
    uint32_t write_reg;
} ID_EX_Latch;

typedef struct {
    bool valid;
    uint32_t pc;
    Instruction instruction;
    Control_Signals control_signals;
    uint32_t alu_result;
    uint32_t rt_value;
    uint32_t write_reg;
} EX_MEM_Latch;

typedef struct {
    bool valid;
    uint32_t pc;
    Instruction instruction;
    Control_Signals control_signals;
    uint32_t alu_result;
    uint32_t rt_value;
    uint32_t write_reg;
} MEM_WB_Latch;

// 해저드 관련 구조체들
typedef struct {
    int forward_a;  // rs 포워딩: 0=no, 1=from_ex_mem, 2=from_mem_wb
    int forward_b;  // rt 포워딩: 0=no, 1=from_ex_mem, 2=from_mem_wb
} ForwardingUnit;

typedef struct {
    bool stall;     // 스톨이 필요한지
    bool flush;     // 플러시가 필요한지
} HazardUnit;

extern IF_ID_Latch if_id_latch;
extern ID_EX_Latch id_ex_latch;
extern EX_MEM_Latch ex_mem_latch;
extern MEM_WB_Latch mem_wb_latch;
extern Registers registers;

// 함수 선언들
extern void stage_IF(void);
extern void stage_ID(void);
extern void stage_EX(void);
extern void stage_MEM(void);
extern void stage_WB(void);

extern void decode_rtype(uint32_t, Instruction*);
extern void decode_itype(uint32_t, Instruction*);
extern void decode_jtype(uint32_t, Instruction*);
extern void extend_imm_val(Instruction*);
extern void setup_control_signals(Instruction*, Control_Signals*);
extern void initialize_control(Control_Signals*);

uint32_t alu_operate(uint32_t, uint32_t, int, Instruction*);

// 해저드 관련 함수 선언들
extern ForwardingUnit detect_forwarding(void);
extern HazardUnit detect_hazard(void);
extern uint32_t get_forwarded_value(int forward_type, uint32_t original_value);
extern void handle_stall(void);
extern void handle_branch_flush(void);