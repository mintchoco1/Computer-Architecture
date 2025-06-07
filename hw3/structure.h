#ifndef STRUCTURE_H
#define STRUCTURE_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define MEMORY_SIZE 0x1000000

extern uint8_t memory[MEMORY_SIZE];

typedef struct {
    uint32_t regs[32];
    uint32_t pc;
} Registers;

typedef struct {
    int alu_ctrl;
    int alu_op;
    int alu_src;
    int mem_read;
    int mem_write;
    int mem_to_reg;
    int reg_wb;
    int reg_dst;
    int get_imm;
    int ex_skip;
    int rt_ch;
    int rs_ch;
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
    uint32_t inst_type;
    uint32_t jump_target;
    uint32_t pc_plus_4;
} Instruction;

typedef struct {
    uint32_t instruction;
    uint32_t pc;
    uint32_t next_pc;  
    bool valid;
    uint32_t reg_src;
    uint32_t reg_tar;
    uint32_t opcode;
    uint32_t funct;
    int forward_a;
    int forward_b;
    uint32_t forward_a_val;
    uint32_t forward_b_val;
} IF_ID_Latch;

typedef struct {
    bool valid;
    uint32_t pc;
    Instruction instruction;
    Control_Signals control_signals;
    uint32_t rs_value;
    uint32_t rt_value;
    uint32_t write_reg;
    uint32_t sign_imm;
    uint32_t shamt;
    int forward_a;
    int forward_b;
    uint32_t forward_a_val;
    uint32_t forward_b_val;
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

typedef struct {
    int forward_a;
    int forward_b;
} ForwardingUnit;

typedef struct {
    bool stall;
    bool flush;
} HazardUnit;

extern IF_ID_Latch if_id_latch;
extern ID_EX_Latch id_ex_latch;
extern EX_MEM_Latch ex_mem_latch;
extern MEM_WB_Latch mem_wb_latch;
extern Registers registers;

// 파이프라인 스테이지
extern void stage_IF(void);
extern void stage_ID(void);
extern void stage_EX(void);
extern void stage_MEM(void);
extern void stage_WB(void);

// 명령어 디코딩
extern void decode_rtype(uint32_t, Instruction*);
extern void decode_itype(uint32_t, Instruction*);
extern void decode_jtype(uint32_t, Instruction*);
extern void extend_imm_val(Instruction*);

// 제어 신호
extern void setup_control_signals(Instruction*, Control_Signals*);
extern void initialize_control(Control_Signals*);

// ALU
extern uint32_t alu_operate(uint32_t, uint32_t, int, Instruction*);

// 해저드 및 포워딩
extern ForwardingUnit detect_forwarding(void);
extern ForwardingUnit detect_branch_forwarding(void);
extern HazardUnit detect_hazard(void);
extern uint32_t get_forwarded_value(int forward_type, uint32_t original_value);
extern void handle_stall(void);
extern void handle_branch_flush(void);

extern void init_branch_predictor(void);
extern bool predict_branch(uint32_t pc);
extern void update_branch_predictor(uint32_t pc, bool actual_taken, bool predicted_taken);
extern void print_branch_prediction_stats(void);
extern void reset_branch_predictor(void);

extern uint64_t branch_predictions;
extern uint64_t branch_correct_predictions;
extern uint64_t branch_mispredictions;

extern const char* get_instruction_name(uint32_t opcode, uint32_t funct);

extern void extend_imm_val(Instruction*);

extern uint64_t g_inst_count;

#endif