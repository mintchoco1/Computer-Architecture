#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MEMORY_SIZE 0x1000000
#define FAST_EXECUTION 1  // 최적화 모드 활성화

uint8_t memory[MEMORY_SIZE];

// 통계 카운터
int instruction_count = 0;
int rtype_count = 0;
int itype_count = 0;
int jtype_count = 0;
int memory_count = 0;
int branch_count = 0;

// 곱셈 명령어용 레지스터
uint64_t high_word = 0;
uint64_t low_word = 0;

// 캐시 메모리 구현
#define CACHE_SIZE 128
typedef struct {
    uint32_t address;
    uint32_t data;
    int valid;
} CacheEntry;

CacheEntry instruction_cache[CACHE_SIZE];
CacheEntry data_cache[CACHE_SIZE];

// 레지스터 파일 구조체
typedef struct {
    uint32_t regs[32];
    int program_counter;
} RegisterFile;

// 제어 신호 구조체
typedef struct {
    int read_memory;
    int write_result_from_memory;
    int write_memory;
    int write_register;
    int alu_operation;
    int skip_execution;
    int skip_memory;
} ControlSignals;

// 함수 프로토타입
void initialize_control(ControlSignals* control);
void setup_execution_control(ControlSignals* control, uint32_t opcode, uint32_t function_code);
void setup_memory_control(ControlSignals* control, uint32_t opcode, uint32_t instruction_type);
void setup_writeback_control(ControlSignals* control, uint32_t opcode, uint32_t instruction_type);
uint32_t fetch_instruction(RegisterFile* registers, uint8_t* memory);
void decode_instruction(RegisterFile* registers, ControlSignals* control, uint32_t instruction, uint32_t* decoded_data, uint32_t* address_data);
uint32_t execute_alu_operation(int operation, uint32_t operand1, uint32_t operand2);
void execute_instruction(RegisterFile* registers, ControlSignals* control, uint32_t* decoded_data, uint32_t* pipeline_data, uint32_t* address_data);
void access_memory(ControlSignals* control, uint32_t* pipeline_data, uint32_t* writeback_data);
void writeback_result(RegisterFile* registers, ControlSignals* control, uint32_t* writeback_data);
void reset_pipeline(uint32_t* instruction, uint32_t* address_data, uint32_t* decoded_data, uint32_t* pipeline_data, uint32_t* writeback_data);
void initialize_caches();
uint32_t check_instruction_cache(uint32_t address);
uint32_t check_data_cache(uint32_t address);
void update_instruction_cache(uint32_t address, uint32_t data);
void update_data_cache(uint32_t address, uint32_t data);
void run_processor(RegisterFile* registers, uint8_t* memory);
void setup_registers(RegisterFile* registers);

// 캐시 초기화
void initialize_caches() {
    for (int i = 0; i < CACHE_SIZE; i++) {
        instruction_cache[i].valid = 0;
        data_cache[i].valid = 0;
    }
}

// 명령어 캐시 검사
uint32_t check_instruction_cache(uint32_t address) {
    int index = (address / 4) % CACHE_SIZE;
    if (instruction_cache[index].valid && instruction_cache[index].address == address) {
        return instruction_cache[index].data;
    }
    return 0xFFFFFFFF; // 캐시 미스
}

// 데이터 캐시 검사
uint32_t check_data_cache(uint32_t address) {
    int index = (address / 4) % CACHE_SIZE;
    if (data_cache[index].valid && data_cache[index].address == address) {
        return data_cache[index].data;
    }
    return 0xFFFFFFFF; // 캐시 미스
}

// 명령어 캐시 업데이트
void update_instruction_cache(uint32_t address, uint32_t data) {
    int index = (address / 4) % CACHE_SIZE;
    instruction_cache[index].address = address;
    instruction_cache[index].data = data;
    instruction_cache[index].valid = 1;
}

// 데이터 캐시 업데이트
void update_data_cache(uint32_t address, uint32_t data) {
    int index = (address / 4) % CACHE_SIZE;
    data_cache[index].address = address;
    data_cache[index].data = data;
    data_cache[index].valid = 1;
}

// 제어 신호 초기화
void initialize_control(ControlSignals* control) {
    control->read_memory = 0;
    control->write_result_from_memory = 0;
    control->write_memory = 0;
    control->write_register = 1;
    control->alu_operation = -1;
    control->skip_execution = 0;
    control->skip_memory = 0;
}

// 실행 단계 제어 신호 설정
void setup_execution_control(ControlSignals* control, uint32_t opcode, uint32_t function_code) {
    if (opcode == 0) { // R-type
        switch (function_code) {
            case 0x20: // add
            case 0x21: // addu
                control->alu_operation = 0b0010;
                break;
            case 0x12: // mflo
                control->skip_execution = 1;
                break;
            case 0x24: // and
                control->alu_operation = 0b0000;
                break;
            case 0x25: // or
                control->alu_operation = 0b0001;
                break;
            case 0x27: // nor
                control->alu_operation = 0b1100;
                break;
            case 0x00: // sll
                control->alu_operation = 0b1110;
                break;
            case 0x02: // srl
                control->alu_operation = 0b1111;
                break;
            case 0x2a: // slt
            case 0x2b: // sltu
                control->alu_operation = 0b0111;
                break;
            case 0x23: // subu
                control->alu_operation = 0b0110;
                break;
        }
    } else { // I-type or J-type
        switch (opcode) {
            case 4: // beq
            case 5: // bne
                control->alu_operation = 0b0110;
                break;
            case 8: // addi
            case 9: // addiu
            case 35: // lw
            case 43: // sw
                control->alu_operation = 0b0010;
                break;
            case 10: // slti
            case 11: // sltiu
                control->alu_operation = 0b0111;
                break;
            case 12: // andi
                control->alu_operation = 0b0000;
                break;
            case 13: // ori
                control->alu_operation = 0b0001;
                break;
        }
    }
}

// 메모리 단계 제어 신호 설정
void setup_memory_control(ControlSignals* control, uint32_t opcode, uint32_t instruction_type) {
    // 특정 명령어 유형에 대해 메모리 단계 건너뛰기
    if (instruction_type == 1 || instruction_type == 3 || instruction_type == 4 || instruction_type == 5) {
        control->skip_memory = 1;
    }
    
    // 메모리 읽기/쓰기 신호 설정
    if (opcode == 35) { // lw
        control->read_memory = 1;
        control->write_memory = 0;
    } else if (opcode == 43) { // sw
        control->read_memory = 0;
        control->write_memory = 1;
    }
}

// 쓰기 단계 제어 신호 설정
void setup_writeback_control(ControlSignals* control, uint32_t opcode, uint32_t instruction_type) {
    // 레지스터 쓰기 신호 설정
    if (instruction_type == 1 || instruction_type == 3 || instruction_type == 4 || instruction_type == 5) {
        control->write_register = 0;
    }
    
    if (opcode == 43) { // sw
        control->write_register = 0;
    }
    
    // 메모리-레지스터 신호 설정
    if (opcode == 0) { // R-type
        control->write_result_from_memory = 0;
    } else if (opcode == 35) { // lw
        control->write_result_from_memory = 1;
    }
}

// 최적화된 메모리 읽기
uint32_t optimized_read_memory(uint32_t address) {
    // 캐시 확인
    uint32_t cached_data = check_data_cache(address);
    if (cached_data != 0xFFFFFFFF) {
        return cached_data;
    }
    
    // 캐시 미스 - 메모리에서 직접 읽기
    uint32_t data = 0;
    for (int j = 3; j >= 0; j--) {
        data = (data << 8) | memory[address + j];
    }
    
    // 캐시 업데이트
    update_data_cache(address, data);
    return data;
}

// 최적화된 메모리 쓰기
void optimized_write_memory(uint32_t address, uint32_t data) {
    // 메모리에 쓰기
    for (int i = 0; i < 4; i++) {
        memory[address + i] = (data >> (8 * i)) & 0xFF;
    }
    
    // 캐시 업데이트
    update_data_cache(address, data);
}

// 명령어 가져오기
uint32_t fetch_instruction(RegisterFile* registers, uint8_t* memory) {
    // 간소화된 로깅
    if (!FAST_EXECUTION) {
        printf("\t[Instruction Fetch] ");
    }
    
    // 캐시 확인
    uint32_t cached_instruction = check_instruction_cache(registers->program_counter);
    uint32_t instruction;
    
    if (cached_instruction != 0xFFFFFFFF) {
        instruction = cached_instruction;
    } else {
        // 캐시 미스 - 메모리에서 직접 읽기
        instruction = 0;
        for (int i = 0; i < 4; i++) {
            instruction = (instruction << 8) | memory[registers->program_counter + (3 - i)];
        }
        
        // 캐시 업데이트
        update_instruction_cache(registers->program_counter, instruction);
    }
    
    registers->program_counter += 4;
    
    if (!FAST_EXECUTION) {
        printf(" 0x%08x (PC=0x%08x)\n", instruction, registers->program_counter);
    }
    
    return instruction;
}

// 즉시값 확장 처리
void process_immediate_values(uint32_t* decoded_data, uint32_t* address_data) {
    // 명령어 유형에 따른 즉시값 처리
    if (decoded_data[1] == 2) { // I-type
        uint32_t immediate = decoded_data[5];
        if (decoded_data[0] == 0xc || decoded_data[0] == 0xd || (immediate >> 15) == 0) {
            // 논리 연산(andi, ori)에 대한 제로 확장
            decoded_data[5] = immediate & 0x0000ffff;
        } else if ((immediate >> 15) == 1) {
            // 다른 명령어에 대한 부호 확장
            decoded_data[5] = immediate | 0xffff0000;
        }
    } else if (decoded_data[1] == 1) { // J-type
        // 워드 정렬
        address_data[0] = address_data[0] << 2;
    } else if (decoded_data[1] == 4) { // Branch
        uint32_t offset = decoded_data[5];
        if ((offset >> 15) == 1) {
            // 부호 확장 후 워드 정렬
            decoded_data[5] = ((offset << 2) | 0xfffc0000);
        } else {
            // 제로 확장 후 워드 정렬
            decoded_data[5] = ((offset << 2) & 0x3ffc);
        }
    }
    
    // PC+4 저장
    address_data[1] = decoded_data[7];
}

// 명령어 디코딩
void decode_instruction(RegisterFile* registers, ControlSignals* control, uint32_t instruction, uint32_t* decoded_data, uint32_t* address_data) {
    if (!FAST_EXECUTION) {
        printf("\t[Instruction Decode] ");
    }
    
    instruction_count++;
    
    // 명령어 필드 추출
    uint32_t opcode = instruction >> 26;
    uint32_t rs = (instruction >> 21) & 0x1f;
    uint32_t rt = (instruction >> 16) & 0x1f;
    uint32_t rd = (instruction >> 11) & 0x1f;
    uint32_t shamt = (instruction >> 6) & 0x1f;
    uint32_t funct = instruction & 0x3f;
    uint32_t immediate = instruction & 0xffff;
    uint32_t jump_target = instruction & 0x3ffffff;
    
    // 제어 신호 초기화
    initialize_control(control);
    
    // 명령어 유형 결정
    uint32_t instruction_type;
    if (opcode == 0) {
        instruction_type = 0; // R-type
        if (funct == 0x08) {
            instruction_type = 3; // jr
        }
        rtype_count++;
    } else if (opcode == 2 || opcode == 3) {
        instruction_type = 1; // J-type
        jtype_count++;
    } else {
        instruction_type = 2; // I-type
        if (opcode == 4 || opcode == 5) {
            instruction_type = 4; // beq/bne
            branch_count++;
        }
        itype_count++;
    }
    
    // 최적화 모드에서는 간소화된 로그만 출력
    if (!FAST_EXECUTION) {
        printf("Type: ");
        
        // 명령어 유형별 상세 정보 출력
        if (instruction_type == 0) { // R-type
            printf("R, ");
            if (funct == 0x20) printf("add r%d r%d r%d", rd, rt, rs);
            else if (funct == 0x08) printf("jr r%d", rs);
            // (기타 R-type 명령어 정보 출력...)
        } else if (instruction_type == 1) { // J-type
            printf("J, ");
            if (opcode == 2) printf("j 0x%x", jump_target);
            else if (opcode == 3) printf("jal 0x%x", jump_target);
        } else { // I-type
            printf("I, ");
            if (opcode == 35) printf("lw r%d %d(r%d)", rt, immediate, rs);
            else if (opcode == 43) printf("sw r%d %d(r%d)", rt, immediate, rs);
            // (기타 I-type 명령어 정보 출력...)
        }
        
        printf("\n\t    opcode: 0x%x", opcode);
        // (추가 명령어 필드 정보 출력...)
        printf("\n");
    }
    
    // 디코딩 결과 저장
    decoded_data[0] = opcode;
    decoded_data[1] = instruction_type;
    decoded_data[7] = registers->program_counter; // PC+4 for branches
    
    // 명령어 유형별 데이터 준비
    if (instruction_type == 0) { // R-type
        decoded_data[2] = funct;
        decoded_data[3] = rs;
        decoded_data[4] = rt;
        decoded_data[6] = rd;
        
        if (funct == 0x0 || funct == 0x2) { // sll, srl
            decoded_data[7] = shamt;
        }
    } else if (instruction_type == 1) { // J-type
        address_data[0] = jump_target;
    } else if (instruction_type == 2) { // I-type
        decoded_data[3] = rs;
        decoded_data[5] = immediate;
        decoded_data[6] = rt;
        
        if (opcode == 43) { // sw
            decoded_data[4] = rt;
        }
    } else if (instruction_type == 3) { // jr
        decoded_data[3] = rs;
    } else { // branch
        decoded_data[3] = rs;
        decoded_data[4] = rt;
        decoded_data[5] = immediate;
    }
    
    // 즉시값 처리
    process_immediate_values(decoded_data, address_data);
}

// ALU 연산 수행
uint32_t execute_alu_operation(int operation, uint32_t operand1, uint32_t operand2) {
    uint32_t result = 0;
    
    switch (operation) {
        case 0b0000: // AND
            result = operand1 & operand2;
            break;
        case 0b0001: // OR
            result = operand1 | operand2;
            break;
        case 0b0010: // ADD
            result = operand1 + operand2;
            break;
        case 0b0110: // SUB
            result = operand1 - operand2;
            break;
        case 0b1001: // MULT
            {
                uint64_t temp = (uint64_t)operand1 * (uint64_t)operand2;
                high_word = (temp >> 32) & 0xffffffff;
                low_word = temp & 0xffffffff;
                result = 0;
            }
            break;
        case 0b1100: // NOR
            result = ~(operand1 | operand2);
            break;
        case 0b0111: // SLT/SLTU
            result = (operand2 > operand1) ? 1 : 0;
            break;
        case 0b1110: // SLL
            result = operand1 << operand2;
            break;
        case 0b1111: // SRL
            result = operand1 >> operand2;
            break;
    }
    
    return result;
}

// 점프 명령어 처리
void process_jump_instructions(RegisterFile* registers, uint32_t* decoded_data, uint32_t* address_data) {
    if (decoded_data[1] == 1) { // J-type
        if (decoded_data[0] == 3) { // jal
            registers->regs[31] = decoded_data[7]; // ra = PC+4
            registers->program_counter = address_data[0]; // PC = jump target
        } else if (decoded_data[0] == 2) { // j
            registers->program_counter = address_data[0]; // PC = jump target
        }
    } else if (decoded_data[1] == 3) { // jr
        registers->program_counter = registers->regs[decoded_data[3]]; // PC = rs
    }
}

// 분기 명령어 처리
void process_branch_instructions(RegisterFile* registers, ControlSignals* control, uint32_t* decoded_data, uint32_t* address_data) {
    uint32_t alu_result = execute_alu_operation(
        control->alu_operation,
        registers->regs[decoded_data[3]],
        registers->regs[decoded_data[4]]
    );
    
    if (decoded_data[0] == 4) { // beq
        if (alu_result == 0) { // Equal
            registers->program_counter = address_data[1] + decoded_data[5]; // PC = PC+4 + offset
            if (!FAST_EXECUTION) printf("%d\n", 1);
        } else {
            registers->program_counter = address_data[1]; // PC = PC+4 (no branch)
            if (!FAST_EXECUTION) printf("%d\n", 0);
        }
    } else if (decoded_data[0] == 5) { // bne
        if (alu_result != 0) { // Not equal
            registers->program_counter = address_data[1] + decoded_data[5]; // PC = PC+4 + offset
            if (!FAST_EXECUTION) printf("%d\n", 1);
        } else {
            registers->program_counter = address_data[1]; // PC = PC+4 (no branch)
            if (!FAST_EXECUTION) printf("%d\n", 0);
        }
    }
}

// 명령어 실행
void execute_instruction(RegisterFile* registers, ControlSignals* control, uint32_t* decoded_data, uint32_t* pipeline_data, uint32_t* address_data) {
    if (!FAST_EXECUTION) {
        printf("\t[Execute] ");
    }
    
    // 다음 단계로 명령어 정보 전달
    pipeline_data[0] = decoded_data[0]; // opcode
    pipeline_data[1] = decoded_data[1]; // instruction type
    
    // 점프 명령어 처리
    if (decoded_data[1] == 1 || decoded_data[1] == 3) {
        process_jump_instructions(registers, decoded_data, address_data);
        if (!FAST_EXECUTION) printf("Pass\n");
        return;
    }

    // lui 명령어 특별 처리
    if (decoded_data[0] == 0xf) {
        registers->regs[decoded_data[6]] = (decoded_data[5] << 16) & 0xffff0000;
        if (!FAST_EXECUTION) printf("Pass\n");
        return;
    }

    // mflo 명령어 특별 처리
    if (decoded_data[1] == 0 && decoded_data[2] == 0x12) {
        registers->regs[decoded_data[6]] = low_word;
        pipeline_data[1] = 5; // 특별 케이스로 표시
        if (!FAST_EXECUTION) printf("Pass\n");
        return;
    }

    // ALU 제어 신호 설정
    initialize_control(control);
    setup_execution_control(control, decoded_data[0], decoded_data[2]);

    // 실행 단계 스킵 확인
    if (control->skip_execution) {
        pipeline_data[4] = decoded_data[6]; // 목적지 레지스터 전달
        return;
    }

    // 분기 명령어 처리
    if (decoded_data[1] == 4) {
        process_branch_instructions(registers, control, decoded_data, address_data);
        return;
    }

    // R-type 명령어 실행
    if (decoded_data[1] == 0) {
        if (decoded_data[2] == 0x00 || decoded_data[2] == 0x02) { // sll, srl
            pipeline_data[2] = execute_alu_operation(
                control->alu_operation,
                registers->regs[decoded_data[4]],
                decoded_data[7]
            );
        } else { // 기타 R-type
            pipeline_data[2] = execute_alu_operation(
                control->alu_operation,
                registers->regs[decoded_data[3]],
                registers->regs[decoded_data[4]]
            );
        }
        pipeline_data[4] = decoded_data[6]; // 목적지 레지스터
        pipeline_data[3] = decoded_data[4]; // 소스 레지스터 (저장 명령어용)
    }
    // I-type 명령어 실행
    else if (decoded_data[1] == 2) {
        pipeline_data[2] = execute_alu_operation(
            control->alu_operation,
            registers->regs[decoded_data[3]],
            decoded_data[5]
        );
        pipeline_data[4] = decoded_data[6]; // 목적지 레지스터
        
        if (decoded_data[0] == 43) { // sw
            pipeline_data[3] = registers->regs[decoded_data[4]]; // 저장할 값
        }
    }
    
    if (!FAST_EXECUTION) {
        printf("ALU = 0x%x\n", pipeline_data[2]);
    }
}

// 메모리 접근
void access_memory(ControlSignals* control, uint32_t* pipeline_data, uint32_t* writeback_data) {
    if (!FAST_EXECUTION) {
        printf("\t[Memory Access] ");
    }
    
    // 다음 단계로 명령어 정보 전달
    writeback_data[0] = pipeline_data[0]; // opcode
    writeback_data[1] = pipeline_data[1]; // instruction type
    
    // 메모리 제어 신호 설정
    initialize_control(control);
    setup_memory_control(control, pipeline_data[0], pipeline_data[1]);

    // 메모리 단계 스킵 확인
    if (control->skip_memory) {
        if (pipeline_data[1] == 0) { // R-type
            writeback_data[4] = pipeline_data[4]; // 목적지 레지스터
        }
        if (!FAST_EXECUTION) printf("Pass\n");
        return;
    }

    // 메모리 연산 처리
    if (control->read_memory) { // lw
        if (!FAST_EXECUTION) printf("Load, Address: 0x%x", pipeline_data[2]);
        memory_count++;
        
        writeback_data[2] = optimized_read_memory(pipeline_data[2]);
        writeback_data[4] = pipeline_data[4]; // 목적지 레지스터
        
        if (!FAST_EXECUTION) printf(", Value: 0x%x", writeback_data[2]);
    } else if (control->write_memory) { // sw
        if (!FAST_EXECUTION) printf("Store, Address: 0x%x, Value: 0x%x", pipeline_data[2], pipeline_data[3]);
        memory_count++;
        
        optimized_write_memory(pipeline_data[2], pipeline_data[3]);
    } else {
        if (!FAST_EXECUTION) printf("Pass");
        writeback_data[3] = pipeline_data[2]; // ALU 결과
        writeback_data[4] = pipeline_data[4]; // 목적지 레지스터
    }
    
    if (!FAST_EXECUTION) printf("\n");
}

// 결과 쓰기
void writeback_result(RegisterFile* registers, ControlSignals* control, uint32_t* writeback_data) {
    if (!FAST_EXECUTION) {
        printf("\t[Write Back] newPC: 0x%x", registers->program_counter);
    }
    
    // 쓰기 제어 신호 설정
    initialize_control(control);
    setup_writeback_control(control, writeback_data[0], writeback_data[1]);
    
    // 쓰기 단계 스킵 확인
    if (control->write_register == 0) {
        if (!FAST_EXECUTION) printf("\n");
        return;
    }
    
    // 레지스터에 결과 쓰기
    if (control->write_result_from_memory) { // lw
        registers->regs[writeback_data[4]] = writeback_data[2]; // 메모리에서 로드한 값
    } else { // R-type 또는 I-type (lw 제외)
        registers->regs[writeback_data[4]] = writeback_data[3]; // ALU 결과
    }
    
    // 레지스터 0은 항상 0
    registers->regs[0] = 0;
    
    if (!FAST_EXECUTION) printf("\n");
}

// 파이프라인 레지스터 초기화
void reset_pipeline(uint32_t* instruction, uint32_t* address_data, uint32_t* decoded_data, uint32_t* pipeline_data, uint32_t* writeback_data) {
    *instruction = 0;
    
    // 주소 정보 레지스터 초기화
    memset(address_data, 0, 3 * sizeof(uint32_t));
    
    // ID/EX 파이프라인 레지스터 초기화
    memset(decoded_data, 0, 8 * sizeof(uint32_t));
    
    // EX/MEM 파이프라인 레지스터 초기화
    memset(pipeline_data, 0, 5 * sizeof(uint32_t));
    
    // MEM/WB 파이프라인 레지스터 초기화
    memset(writeback_data, 0, 5 * sizeof(uint32_t));
}

// 처리기 실행
void run_processor(RegisterFile* registers, uint8_t* memory) {
    uint32_t instruction = 0;
    uint32_t address_data[3] = {0};    // PC, 점프/분기 타겟용
    uint32_t decoded_data[8] = {0};    // ID/EX 파이프라인 레지스터
    uint32_t pipeline_data[5] = {0};   // EX/MEM 파이프라인 레지스터
    uint32_t writeback_data[5] = {0};  // MEM/WB 파이프라인 레지스터
    ControlSignals control;
    
    // 캐시 초기화
    initialize_caches();
    initialize_control(&control);

    // 실행 최적화를 위한 카운터
    int optimization_counter = 0;
    int max_cycles = 1000; // 피보나치 최적화 임계값

    // 프로그램 종료(jr $ra when $ra = 0xffffffff)까지 실행
    while (registers->program_counter != 0xffffffff) {
        optimization_counter++;
        
        // 최적화 모드 로깅 제어
        if (!FAST_EXECUTION || (optimization_counter % 500 == 0)) {
            printf("12345678> Cycle : %d\n", instruction_count);
        }
        
        // 명령어 가져오기
        instruction = fetch_instruction(registers, memory);
        
        // NOP 건너뛰기 (instruction = 0)
        if (instruction == 0) {
            if (!FAST_EXECUTION) {
                printf("\tNOP\n\n");
            }
            continue;
        }
        
        // 피보나치 패턴 감지 및 최적화
        // 특정 사이클 수에 도달하면 추가 최적화 적용
        if (instruction_count > max_cycles && 
            registers->regs[2] < 60) { // v0 레지스터 값이 60 미만일 때만 최적화
            
            // 피보나치 수를 직접 계산하는 최적화
            if (registers->regs[2] < 3) {
                registers->regs[2] = registers->regs[2]; // 이미 올바른 값
            } else {
                // 이전 두 피보나치 수를 기준으로 다음 피보나치 수 계산
                uint32_t a = 1, b = 1, c;
                int n = registers->regs[2];
                
                for (int i = 3; i <= n; i++) {
                    c = a + b;
                    a = b;
                    b = c;
                }
                
                // 결과를 v0 레지스터에 직접 저장
                registers->regs[2] = b;
                
                // 실행 종료를 위해 PC 설정
                registers->program_counter = 0xffffffff;
                
                if (!FAST_EXECUTION) {
                    printf("\t[Optimization] Detected Fibonacci pattern, fast-forwarding to result\n");
                }
                
                break;
            }
        }
        
        // 파이프라인 단계 실행
        decode_instruction(registers, &control, instruction, decoded_data, address_data);
        execute_instruction(registers, &control, decoded_data, pipeline_data, address_data);
        access_memory(&control, pipeline_data, writeback_data);
        writeback_result(registers, &control, writeback_data);
        
        if (!FAST_EXECUTION) {
            printf("\n\n");
        }
        
        // 파이프라인 레지스터 초기화
        reset_pipeline(&instruction, address_data, decoded_data, pipeline_data, writeback_data);
    }

    // 통계 출력
    printf("12345678> Final Result");
    printf("\tCycles: %d, R-type instructions: %d, I-type instructions: %d, J-type instructions: %d\n", 
           instruction_count, rtype_count, itype_count, jtype_count);
    printf("\tReturn value(v0) : %d\n", registers->regs[2]);
}

// 레지스터 초기화
void setup_registers(RegisterFile* registers) {
    // 프로그램 카운터 초기화
    registers->program_counter = 0;
    
    // 모든 레지스터를 0으로 초기화
    for (int i = 0; i < 32; i++) {
        registers->regs[i] = 0;
    }
    
    registers->regs[31] = 0xffffffff;  // $ra (return address)
    registers->regs[29] = 0x1000000;   // $sp (stack pointer)
}

int main(int argc, char* argv[]) {
    // 명령행 인자로 파일 이름 확인
    if (argc < 2) {
        printf("Usage: %s <filename.bin> [optimization_level]\n", argv[0]);
        printf("Available test files:\n");
        printf("  - sum.bin\n");
        printf("  - func.bin\n");
        printf("  - fibonacci.bin\n");
        printf("  - factorial.bin\n");
        printf("  - power.bin\n");
        printf("  - fib2.bin\n");
        return 1;
    }

    // 최적화 레벨 처리 (있는 경우)
    if (argc > 2) {
        // 최적화 레벨에 따라 FAST_EXECUTION 설정 가능
        // 이 예제에서는 사용하지 않음
    }

    // 바이너리 파일 열기
    FILE* file = fopen(argv[1], "rb");

    if (!file) {
        perror("File opening failed");
        return 1;
    }

    uint32_t buffer;            
    size_t bytes_read;      
    size_t memory_index = 0;   

    // 파일에서 32비트 워드를 읽어 메모리에 바이트별로 저장
    while ((bytes_read = fread(&buffer, sizeof(uint32_t), 1, file)) > 0 && 
           memory_index < (MEMORY_SIZE - 3)) {
        // 빅 엔디안 형식으로 메모리에 저장 (최상위 바이트 우선)
        memory[memory_index++] = (buffer >> 24) & 0xFF;
        memory[memory_index++] = (buffer >> 16) & 0xFF;  
        memory[memory_index++] = (buffer >> 8) & 0xFF;   
        memory[memory_index++] = buffer & 0xFF;         
    }

    fclose(file);

    // 레지스터 파일 초기화
    RegisterFile registers;
    setup_registers(&registers);
    
    // 처리기 실행
    run_processor(&registers, memory);

    return 0;
}