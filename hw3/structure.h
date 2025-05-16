#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define MEMORY_SIZE 0x1000000

uint8_t memory[MEMORY_SIZE];

int instruction_count = 0;
int rtype_count = 0;
int itype_count = 0;
int jtype_count = 0;
int memory_count = 0;
int branch_count = 0;

uint64_t high_word = 0;
uint64_t low_word = 0;

typedef struct {
    uint32_t regs[32];
    int program_counter;
} Registers;

typedef struct {
    int reg_dst;        
    int reg_write;        
    int alu_src;         
    int mem_to_reg;    
    int mem_read;          
    int mem_write;         
    int branch;             
    int jump;             
    int alu_op;           
} ControlSignals;

typedef struct {
    uint32_t opcode;     
    uint32_t rs;       
    uint32_t rt;            
    uint32_t rd;           
    uint32_t rs_value;     
    uint32_t rt_value;     
    uint32_t immediate;    
    uint32_t shamt;         
    uint32_t funct;       
    uint32_t jump_target; 
    uint32_t inst_type;//r, i, j, jr, branch, jalr
    uint32_t pc_plus_4;  
} InstructionInfo;

//==========latch==========
//각 스테이지는 이전 단계의 latch 구조체를 읽고, 현재 단계의 출력 latch 구조체에 값을 씀
//cycle 끝에 한 번에 latch를 업데이트 해야함
typedef struct {
    uint32_t pc;
    uint32_t instruction;
} IF_ID_Latch;

typedef struct {
    uint32_t pc;
    uint32_t rs_value, rt_value;//실제 계산에 사용
    uint32_t rs, rt, rd;//forwarding 판단용 또는 목적지 레지스터 전달
    uint32_t immediate;
    uint32_t shamt, funct;
    ControlSignals control;
} ID_EX_Latch;

typedef struct {
    uint32_t pc;
    uint32_t alu_result;
    uint32_t rt_value;//sw 시에 메모리에 쓸 데이터
    uint32_t write_reg;//목적지 레지스터 번호
    ControlSignals control;
} EX_MEM_Latch;

typedef struct {
    uint32_t alu_result;//연산 결과
    uint32_t mem_data;//메모리에서 읽은 값 (lw일 때)
    uint32_t write_reg;//목적지 레지스터
    ControlSignals control;
} MEM_WB_Latch;

//==========초기화===========
void initialize_control(ControlSignals *control);
void init_registers(Registers* registers);
void setup_control_signals(InstructionInfo* info, ControlSignals* control);

//==========fetch===========
void instruction_fetch(Registers* registers, uint8_t* memory);

//==========decode===========
void decode_rtype(uint32_t instruction, InstructionInfo *info);
void decode_itype(uint32_t instruction, InstructionInfo* info);
void decode_jtype(uint32_t instruction, InstructionInfo *info);
void display_rtype(InstructionInfo *info);
void display_itype(InstructionInfo *info);
void display_jtype(InstructionInfo *info);
void instruction_decode(uint32_t instruction, Registers* registers, InstructionInfo* info, ControlSignals* control);

void extend_immediate_values(InstructionInfo *info);
uint32_t alu_operation(int alu_op, uint32_t operand1, uint32_t operand2);
uint32_t select_alu_operation(InstructionInfo *info, ControlSignals *control, uint32_t operand1, uint32_t operand2);
uint32_t read_from_memory(uint32_t address);
void write_to_memory(uint32_t address, uint32_t data);
uint32_t memory_access(uint32_t address, uint32_t write_data, ControlSignals *control, InstructionInfo *info);
uint32_t execute_instruction(InstructionInfo* info, ControlSignals* control, Registers* registers);
uint32_t memory_access(uint32_t address, uint32_t write_data, ControlSignals* control, InstructionInfo* info);
void write_back(uint32_t alu_result, uint32_t memory_data, InstructionInfo* info, ControlSignals* control, Registers* registers);
void run_processor(Registers* registers, uint8_t* memory);

void execute_cycle(Registers *registers, uint8_t *memory);
void run_processor(Registers *registers, uint8_t *memory);
