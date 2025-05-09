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

uint32_t instruction_fetch(Registers* registers, uint8_t* memory);
void instruction_decode(uint32_t instruction, Registers* registers, InstructionInfo* info, ControlSignals* control);
uint32_t alu_operation(int alu_op, uint32_t operand1, uint32_t operand2);
uint32_t execute_instruction(InstructionInfo* info, ControlSignals* control, Registers* registers);
uint32_t memory_access(uint32_t address, uint32_t write_data, ControlSignals* control, InstructionInfo* info);
void write_back(uint32_t alu_result, uint32_t memory_data, InstructionInfo* info, ControlSignals* control, Registers* registers);
void run_processor(Registers* registers, uint8_t* memory);
void init_registers(Registers* registers);
void print_registers(Registers* registers);

void initialize_control(ControlSignals* control) {
    control->reg_dst = 0;
    control->reg_write = 0;
    control->alu_src = 0;
    control->mem_to_reg = 0;
    control->mem_read = 0;
    control->mem_write = 0;
    control->branch = 0;
    control->jump = 0;
    control->alu_op = 0;
}


void setup_control_signals(InstructionInfo* info, ControlSignals* control) {
    initialize_control(control);

    switch (info->opcode) {
        case 0: // R-type
            control->reg_dst = 1;       // rd를 목적지 레지스터로 사용
            control->reg_write = 1;     // 레지스터 쓰기 활성화
            control->alu_op = 2;        // R-type ALU 연산
            if (info->funct == 0x08) {  // jr
                control->jump = 1;
                control->reg_write = 0; // 레지스터 쓰기 비활성화
            }
            else if (info->funct == 0x09) { // jalr
                control->jump = 1;
                control->reg_write = 1; // PC+4를 rd에 저장
            }
            break;        
        case 2: // j
            control->jump = 1;
            control->reg_write = 0;     // 레지스터 쓰기 비활성화
            break;
        case 3: // jal
            control->jump = 1;
            control->reg_write = 0;     // $ra에 쓰기 위해 활성화
            break;
        case 4: // beq
        case 5: // bne
            control->branch = 1;
            control->alu_op = 1;        // 뺄셈 연산 (비교용)
            control->reg_write = 0;     // 레지스터 쓰기 비활성화
            break;
        case 8: // addi
        case 9: // addiu
            control->alu_src = 1;       // immediate 값 사용
            control->reg_write = 1;     // 레지스터 쓰기 활성화
            break;
        case 10: // slti
        case 11: // sltiu
            control->alu_src = 1;       // immediate 값 사용
            control->reg_write = 1;     // 레지스터 쓰기 활성화
            control->alu_op = 3;        // 비교 연산
            break;
        case 12: // andi
            control->alu_src = 1;       // immediate 값 사용
            control->reg_write = 1;     // 레지스터 쓰기 활성화
            control->alu_op = 4;        // AND 연산
            break;
        case 13: // ori
            control->alu_src = 1;       // immediate 값 사용
            control->reg_write = 1;     // 레지스터 쓰기 활성화
            control->alu_op = 5;        // OR 연산
            break;
        case 15: // lui
            control->alu_src = 1;       // immediate 값 사용
            control->reg_write = 1;     // 레지스터 쓰기 활성화
            control->alu_op = 6;        // LUI 연산
            break;
        case 35: // lw
            control->alu_src = 1;       // immediate 값 사용
            control->mem_to_reg = 1;    // 메모리 데이터 사용
            control->reg_write = 1;     // 레지스터 쓰기 활성화
            control->mem_read = 1;      // 메모리 읽기 활성화
            break;
        case 43: // sw
            control->alu_src = 1;       // immediate 값 사용
            control->mem_write = 1;     // 메모리 쓰기 활성화
            control->reg_write = 0;     // 레지스터 쓰기 비활성화
            break;
    }
}

uint32_t instruction_fetch(Registers* registers, uint8_t* memory) {
    printf("\t[Fetch] ");
    uint32_t instruction = 0;
    int pc = registers->program_counter;
    
    for (int i = 0; i < 4; i++) {
        instruction = instruction << 8;
        instruction |= memory[pc + (3 - i)];
    }
    
    registers->program_counter += 4;
    printf(" 0x%08x (PC=0x%08x)\n", instruction, registers->program_counter);
    return instruction;
}

void decode_rtype(uint32_t instruction, InstructionInfo* info) {
    info->rs = (instruction >> 21) & 0x1f;     // rs [25-21]
    info->rt = (instruction >> 16) & 0x1f;     // rt [20-16]
    info->rd = (instruction >> 11) & 0x1f;     // rd [15-11]
    info->shamt = (instruction >> 6) & 0x1f;   // shamt [10-6]
    info->funct = instruction & 0x3f;          // funct [5-0]
    info->inst_type = 0;                       // R-type
    
    if (info->funct == 0x08) {
        info->inst_type = 3;                   // jr
    }
    else if (info->funct == 0x09) {
        info->inst_type = 5;                   // jalr
    }
}

void decode_itype(uint32_t instruction, InstructionInfo* info) {
    info->rs = (instruction >> 21) & 0x1f;     // rs [25-21]
    info->rt = (instruction >> 16) & 0x1f;     // rt [20-16]
    info->immediate = instruction & 0xffff;    // immediate [15-0]
    info->inst_type = 2;                       // I-type
    
    if (info->opcode == 4 || info->opcode == 5) {
        info->inst_type = 4;                   // branch
    }
}

void decode_jtype(uint32_t instruction, InstructionInfo* info) {
    info->jump_target = instruction & 0x3ffffff; // target [25-0]
    info->inst_type = 1;                         // J-type
}

void display_rtype(InstructionInfo* info) {
    printf("R, Inst: ");
    
    // funct 코드에 따라 명령어 출력
    switch (info->funct) {
        case 0x08: printf("jr r%d", info->rs); break;
        case 0x09: printf("jalr r%d r%d", info->rd, info->rs); break;
        case 0x20: printf("add r%d r%d r%d", info->rd, info->rs, info->rt); break;
        case 0x22: printf("sub r%d r%d r%d", info->rd, info->rs, info->rt); break;
        case 0x24: printf("and r%d r%d r%d", info->rd, info->rs, info->rt); break;
        case 0x25: printf("or r%d r%d r%d", info->rd, info->rs, info->rt); break;
        case 0x27: printf("nor r%d r%d r%d", info->rd, info->rs, info->rt); break;
        case 0x00: printf("sll r%d r%d %d", info->rd, info->rt, info->shamt); break;
        case 0x02: printf("srl r%d r%d %d", info->rd, info->rt, info->shamt); break;
        case 0x2a: printf("slt r%d r%d r%d", info->rd, info->rs, info->rt); break;
        case 0x2b: printf("sltu r%d r%d r%d", info->rd, info->rs, info->rt); break;
        case 0x23: printf("subu r%d r%d r%d", info->rd, info->rs, info->rt); break;
        case 0x21: printf("addu r%d r%d r%d", info->rd, info->rs, info->rt); break;
        case 0x18: printf("mult r%d r%d", info->rs, info->rt); break;
        case 0x12: printf("mflo r%d", info->rd); break;
        default: printf("unknown R-type (funct: 0x%x)", info->funct); break;
    }
}

void display_itype(InstructionInfo* info) {
    printf("I, Inst: ");
    
    switch (info->opcode) {
        case 4: printf("beq r%d r%d %x", info->rs, info->rt, info->immediate); break;
        case 5: printf("bne r%d r%d %x", info->rs, info->rt, info->immediate); break;
        case 8: printf("addi r%d r%d %d", info->rt, info->rs, info->immediate); break;
        case 9: printf("addiu r%d r%d %d", info->rt, info->rs, info->immediate); break;
        case 10: printf("slti r%d r%d %d", info->rt, info->rs, info->immediate); break;
        case 11: printf("sltiu r%d r%d %d", info->rt, info->rs, info->immediate); break;
        case 12: printf("andi r%d r%d %d", info->rt, info->rs, info->immediate); break;
        case 13: printf("ori r%d r%d %d", info->rt, info->rs, info->immediate); break;
        case 35: printf("lw r%d %d(r%d)", info->rt, info->immediate, info->rs); break;
        case 43: printf("sw r%d %d(r%d)", info->rt, info->immediate, info->rs); break;
        case 15: printf("lui r%d %d", info->rt, info->immediate); break;
        default: printf("unknown I-type (opcode: 0x%x)", info->opcode); break;
    }
}

void display_jtype(InstructionInfo* info) {
    printf("J, Inst: ");
    
    if (info->opcode == 2) {
        printf("j 0x%x", info->jump_target);
    } else if (info->opcode == 3) {
        printf("jal 0x%x", info->jump_target);
    } else {
        printf("unknown J-type (opcode: 0x%x)", info->opcode);
    }
}

void extend_immediate_values(InstructionInfo* info) {
    if (info->inst_type == 2) { // I-type
        if (info->opcode == 12 || info->opcode == 13) {
            info->immediate &= 0x0000ffff;
        } 
        else if ((info->immediate >> 15) == 1) {
            info->immediate |= 0xffff0000;
        }
    } else if (info->inst_type == 4) { // branch
        uint32_t orig_imm = info->immediate;
        
        info->immediate <<= 2; // 4배 (워드 정렬)
        if ((orig_imm >> 15) == 1) {
            info->immediate |= 0xfffc0000;
        }
    } else if (info->inst_type == 1) { // J-type
        info->jump_target <<= 2;
    }
}

void instruction_decode(uint32_t instruction, Registers* registers, InstructionInfo* info, ControlSignals* control) {
    printf("\t[Decode] ");
    instruction_count++;
    
    memset(info, 0, sizeof(InstructionInfo));
    
    info->pc_plus_4 = registers->program_counter - 4;
    
    // opcode 추출 [31-26]
    info->opcode = instruction >> 26;
    
    if (info->opcode == 0) { // R-type
        decode_rtype(instruction, info);
        rtype_count++;
    } else if (info->opcode == 2 || info->opcode == 3) { // J-type
        decode_jtype(instruction, info);
        jtype_count++;
    } else { // I-type
        decode_itype(instruction, info);
        itype_count++;
    }
    
    info->rs_value = registers->regs[info->rs];
    info->rt_value = registers->regs[info->rt];

    setup_control_signals(info, control);
    
    printf("Type: ");
    if (info->inst_type == 0 || info->inst_type == 3 || info->inst_type == 5) { // R-type/jr/jalr
        display_rtype(info);
    } else if (info->inst_type == 1) { // J-type
        display_jtype(info);
    } else { // I-type or branch
        display_itype(info);
    }
    
    printf("\n\t    opcode: 0x%x", info->opcode);
    
    if (info->inst_type == 0 || info->inst_type == 5) { // R-type 또는 jalr
        if (info->funct != 0x08 && info->funct != 0x12 && info->funct != 0x18) {
            printf(", rs: %d (0x%x), rt: %d (0x%x), rd: %d", 
                   info->rs, info->rs_value, info->rt, info->rt_value, info->rd);
                   
            if (info->funct == 0 || info->funct == 2) {
                printf(", shmat: %d", info->shamt);
            }
        } else if (info->funct == 0x08) { // jr
            printf(", rs: %d (0x%x)", info->rs, info->rs_value);
        } else if (info->funct == 0x09) { // jalr
            printf(", rs: %d (0x%x), rd: %d", info->rs, info->rs_value, info->rd);
        } else if (info->funct == 0x12) { // mflo
            printf(", rd: %d", info->rd);
        } else if (info->funct == 0x18) { // mult
            printf(", rs: %d (0x%x), rt: %d (0x%x)", 
                   info->rs, info->rs_value, info->rt, info->rt_value);
        }
        printf(", funct: 0x%x", info->funct);
    } else if (info->inst_type == 1) { // J-type
        printf(", imm: %d", info->jump_target);
    } else if (info->opcode == 15) { // lui
        printf(", rt: %d", info->rt);
    } else { // I-type
        printf(", rs: %d (0x%x), rt: %d (0x%x), imm: %d", 
               info->rs, info->rs_value, info->rt, info->rt_value, info->immediate);
    }
    printf("\n");
    
    printf("\t    RegDst: %d, RegWrite: %d, ALUSrc: %d, PCSrc: %d, MemRead: %d, MemWrite: %d, MemtoReg: %d, ALUOp: %d\n", control->reg_dst, control->reg_write, control->alu_src, (control->branch || control->jump), control->mem_read, control->mem_write, control->mem_to_reg, control->alu_op);

    extend_immediate_values(info);
}

uint32_t alu_operation(int alu_op, uint32_t operand1, uint32_t operand2) {
    uint32_t result = 0;
    
    switch (alu_op) {
        case 0: // 더하기 (ADD)
            result = operand1 + operand2;
            break;
        case 1: // 빼기 (SUB)
            result = operand1 - operand2;
            break;
        case 2: // AND
            result = operand1 & operand2;
            break;
        case 3: // OR
            result = operand1 | operand2;
            break;
        case 4: // XOR
            result = operand1 ^ operand2;
            break;
        case 5: // NOR
            result = ~(operand1 | operand2);
            break;
        case 6: // SLT (Set Less Than)
            result = ((int32_t)operand1 < (int32_t)operand2) ? 1 : 0;
            break;
        case 7: // SLL (Shift Left Logical)
            result = operand1 << operand2;
            break;
        case 8: // SRL (Shift Right Logical)
            result = operand1 >> operand2;
            break;
        case 9: // MULT
            {
                uint64_t temp = (uint64_t)operand1 * (uint64_t)operand2;
                high_word = (temp >> 32) & 0xffffffff;
                low_word = temp & 0xffffffff;
                result = 0;
            }
            break;
    }
    
    return result;
}

uint32_t select_alu_operation(InstructionInfo* info, ControlSignals* control, uint32_t operand1, uint32_t operand2) {
    int alu_function = 0;
    
    if (info->inst_type == 0 || info->inst_type == 5) { // R-type 또는 jalr
        switch (info->funct) {
            case 0x20: // add
            case 0x21: // addu
                alu_function = 0; // 더하기
                break;
            case 0x22: // sub
            case 0x23: // subu
                alu_function = 1; // 빼기
                break;
            case 0x24: // and
                alu_function = 2; // AND
                break;
            case 0x25: // or
                alu_function = 3; // OR
                break;
            case 0x26: // xor
                alu_function = 4; // XOR
                break;
            case 0x27: // nor
                alu_function = 5; // NOR
                break;
            case 0x2a: // slt
            case 0x2b: // sltu
                alu_function = 6; // SLT
                break;
            case 0x00: // sll
                alu_function = 7; // SLL
                break;
            case 0x02: // srl
                alu_function = 8; // SRL
                break;
            case 0x18: // mult
                alu_function = 9; // MULT
                break;
            case 0x09: // jalr
                return 0; 
        }
    } else { // I-type 
        switch (info->opcode) {
            case 4: // beq
            case 5: // bne
                alu_function = 1;
                break;
            case 8: // addi
            case 9: // addiu
            case 35: // lw
            case 43: // sw
                alu_function = 0; // 더하기
                break;
            case 10: // slti
            case 11: // sltiu
                alu_function = 6; // SLT
                break;
            case 12: // andi
                alu_function = 2; // AND
                break;
            case 13: // ori
                alu_function = 3; // OR
                break;
            case 15: // lui
                return (operand2 << 16); // 특별 처리
        }
    }
    
    return alu_operation(alu_function, operand1, operand2);
}

uint32_t execute_instruction(InstructionInfo* info, ControlSignals* control, Registers* registers) {
    printf("\t[Execute] ");
    
    uint32_t alu_result = 0;
    
    // 점프 명령어 처리
    if (control->jump) {
        if (info->inst_type == 1) { // J-type
            if (info->opcode == 3) { // jal
                registers->regs[31] = registers->program_counter; // ra = PC+4
            }
            registers->program_counter = ((info->pc_plus_4 & 0xf0000000) | info->jump_target);
            printf("Jump to 0x%x\n", registers->program_counter);
        } else if (info->inst_type == 3) { // jr
            registers->program_counter = info->rs_value; // PC = rs
            printf("Jump (jr) to 0x%x\n", registers->program_counter);
        } else if (info->inst_type == 5) { // jalr
            uint32_t target_addr = info->rs_value;
            if (info->rd != 0) {
                registers->regs[info->rd] = registers->program_counter;
            }
            registers->program_counter = target_addr;
            printf("Jump (jalr) to 0x%x, return addr: 0x%x\n", target_addr, registers->program_counter);
        }
        return 0;
    }
    
    if (info->opcode == 15) { // lui
        alu_result = info->immediate << 16;
        printf("LUI = 0x%x\n", alu_result);
        return alu_result;
    }
    
    if (info->inst_type == 0 && info->funct == 0x12) { // mflo
        alu_result = low_word;
        printf("MFLO = 0x%x\n", alu_result);
        return alu_result;
    }
    
    uint32_t operand1, operand2;
    
    if (info->inst_type == 0 && (info->funct == 0x00 || info->funct == 0x02)) { // sll, srl
        operand1 = info->rt_value;
        operand2 = info->shamt;
    } else {
        operand1 = info->rs_value;
    
        if (control->alu_src) {
            operand2 = info->immediate; // immediate 값 사용
        } else {
            operand2 = info->rt_value;  // 레지스터 값 사용
        }
    }
    
    alu_result = select_alu_operation(info, control, operand1, operand2);
    
    if (control->branch) {
        bool branch_taken = false;
        
        if (info->opcode == 4) { // beq
            branch_taken = (alu_result == 0); // 같으면 분기
        } else if (info->opcode == 5) { // bne
            branch_taken = (alu_result != 0); // 다르면 분기
        }
        
        if (branch_taken) {
            registers->program_counter = registers->program_counter + info->immediate;
            printf("Branch Taken: PC = 0x%x, condition = %d\n", 
                   registers->program_counter, branch_taken);
        } else {
            printf("Branch Not Taken: PC = 0x%x, condition = %d\n", 
                   registers->program_counter, branch_taken);
        }
        branch_count++;
    } else {
        printf("ALU = 0x%x\n", alu_result);
    }
    
    return alu_result;
}

uint32_t read_from_memory(uint32_t address) {
    uint32_t data = 0;
    for (int j = 3; j >= 0; j--) {
        data <<= 8;
        data |= memory[address + j];
    }
    return data;
}

void write_to_memory(uint32_t address, uint32_t data) {
    for (int i = 0; i < 4; i++) {
        memory[address + i] = (data >> (8 * i)) & 0xFF;
    }
}

uint32_t memory_access(uint32_t address, uint32_t write_data, ControlSignals* control, InstructionInfo* info) {
    printf("\t[Memory Access] ");
    
    uint32_t memory_data = 0;
    
    if (control->jump || control->branch || (info->inst_type == 0 && info->funct == 0x12) ||(info->inst_type == 5)) { // jr, branch, mflo, jalr
        printf("Pass\n");
        return 0;
    }
    
    if (control->mem_read) {
        memory_data = read_from_memory(address);
        printf("Load, Address: 0x%x, Value: 0x%x\n", address, memory_data);
        memory_count++;
    }
    else if (control->mem_write) {
        write_to_memory(address, write_data);
        printf("Store, Address: 0x%x, Value: 0x%x\n", address, write_data);
        memory_count++;
    }
    else {
        printf("Pass\n");
    }
    
    return memory_data;
}

void write_back(uint32_t alu_result, uint32_t memory_data, InstructionInfo* info, ControlSignals* control, Registers* registers) {
    printf("\t[Write Back] newPC: 0x%x", registers->program_counter);
    
    if (!control->reg_write) {
        printf("\n");
        return;
    }
    
    uint32_t write_reg;
    if (control->reg_dst) {
        write_reg = info->rd; // R-type (rd 사용)
    } else {
        if (info->opcode == 3) { // jal
            write_reg = 31; // $ra = 31
        } else {
            write_reg = info->rt; // I-type (rt 사용)
        }
    }
    
    uint32_t write_data;
    if (control->mem_to_reg) {
        write_data = memory_data; // 메모리에서 읽은 값 사용
    } else {
        write_data = alu_result; // ALU 결과 사용
    }
    
    // 레지스터에 쓰기 (r0는 항상 0)
    if (write_reg != 0) {
        registers->regs[write_reg] = write_data;
        printf(", R%d = 0x%x", write_reg, write_data);
    }
    
    printf("\n");
}

void execute_cycle(Registers* registers, uint8_t* memory) {
    InstructionInfo info;
    ControlSignals control;
    
    uint32_t instruction = instruction_fetch(registers, memory);
    
    if (instruction == 0) {
        printf("\tNOP\n\n");
        return;
    }
    
    instruction_decode(instruction, registers, &info, &control);
    uint32_t alu_result = execute_instruction(&info, &control, registers);
    uint32_t memory_data = memory_access(alu_result, info.rt_value, &control, &info);
    write_back(alu_result, memory_data, &info, &control, registers);
    printf("\n");
}

void run_processor(Registers* registers, uint8_t* memory) {
    while (registers->program_counter != 0xffffffff) {
        printf("================================\n");
        printf("Cycle : %d\n", instruction_count);
        
        execute_cycle(registers, memory);
    }

    printf("===== Final Result =====\n");
    printf("Cycles: %d, R-type instructions: %d, I-type instructions: %d, J-type instructions: %d\n", instruction_count, rtype_count, itype_count, jtype_count);
    printf("Memory operations: %d, Branch instructions: %d\n", memory_count, branch_count);
    printf("Return value(v0) : %d (0x%x)\n", registers->regs[2], registers->regs[2]);
}

void init_registers(Registers* registers) {
    registers->program_counter = 0;
    for (int i = 0; i < 32; i++) {
        registers->regs[i] = 0;
    }
    registers->regs[31] = 0xffffffff;  // $ra 
    registers->regs[29] = 0x1000000;   // $sp 
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <filename.bin>\n", argv[0]);
        return 1;
    }
    FILE* file = fopen(argv[1], "rb");

    if (!file) {
        perror("File opening failed");
        return 1;
    }

    uint32_t buffer;            
    size_t bytes_read;      
    size_t memory_index = 0;   

    while ((bytes_read = fread(&buffer, sizeof(uint32_t), 1, file)) > 0 && 
           memory_index < (MEMORY_SIZE - 3)) {
        // 빅엔디안 형식으로 메모리에 저장 (가장 중요한 바이트 먼저)
        memory[memory_index++] = (buffer >> 24) & 0xFF;
        memory[memory_index++] = (buffer >> 16) & 0xFF;  
        memory[memory_index++] = (buffer >> 8) & 0xFF;   
        memory[memory_index++] = buffer & 0xFF;         
    }
    fclose(file);

    Registers registers;
    init_registers(&registers);
    run_processor(&registers, memory);

    return 0;
}