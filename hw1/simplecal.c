#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define Max_REGISTERS 10

void update_register(const char *operand1, const char *operand2);

int registers[Max_REGISTERS] = {0}; // 모든 레지스터 0으로 초기화
int count = 0;

typedef struct {
    char opcode[10];
    char operand1[20];
    char operand2[20];
} Instruction;

// 문자열 파싱 16진수인지 레지스터인지 확인
int get_operand_value(const char* operand) {
    int value = 0;
    if (operand[0] == 'R') {
        int reg_index = atoi(operand + 1); // "R0"에서 "0" 추출 (0-based 인덱스)
        value = registers[reg_index];
    } else if (sscanf(operand, "%x", &value) == 1) {
        return value;
    }
    return value;
}

void Simple_Math(const Instruction *instruction){ 
    int operand1_value = get_operand_value(instruction->operand1);
    int operand2_value = get_operand_value(instruction->operand2);

    if(strcmp(instruction->opcode, "+") == 0){
        printf("operand1_value: %d, operand2_value: %d\n", operand1_value, operand2_value);
        registers[0] = operand1_value + operand2_value;
        printf("result: %d\n", registers[0]);
    }
    else if(strcmp(instruction->opcode, "-") == 0){
        printf("operand1_value: %d, operand2_value: %d\n", operand1_value, operand2_value);
        registers[0] = operand1_value - operand2_value;
        printf("result: %d\n", registers[0]);
    }
    else if(strcmp(instruction->opcode, "*") == 0){
        printf("operand1_value: %d, operand2_value: %d\n", operand1_value, operand2_value);
        registers[0] = operand1_value * operand2_value;
        printf("result: %d\n", registers[0]);
    }
    else if(strcmp(instruction->opcode, "/") == 0){
        printf("operand1 value: %d, operand2 value: %d\n", operand1_value, operand2_value);
        if(operand2_value != 0){
            registers[0] = operand1_value / operand2_value;
            printf("result: %d\n", registers[0]);
        }
        else{
            printf("Divide by zero error\n");
        }
    }
}

void load_word(const Instruction *instruction){
    int reg_index = atoi(instruction->operand1 + 1); // "R0"에서 "0" 추출 (0-based 인덱스)
    int address = get_operand_value(instruction->operand2);
    registers[reg_index] = address; // 간단히 주소 값을 레지스터에 저장
    printf("Loaded value from address 0x%x into R%d\n", address, reg_index);
}

void store_word(const Instruction *instruction){
    int reg_index = atoi(instruction->operand1 + 1); // "R0"에서 "0" 추출 (0-based 인덱스)
    int address = get_operand_value(instruction->operand2);
    printf("Stored value %d from R%d into address 0x%x\n", registers[reg_index], reg_index, address);
}

void compare_operand(const Instruction *instruction){
    int operand1_value = get_operand_value(instruction->operand1);
    int operand2_value = get_operand_value(instruction->operand2);

    if (operand1_value >= operand2_value) {
        registers[0] = 0;
    } else {
        registers[0] = 1;
    }
    printf("Compared %d and %d, R0 set to %d\n", operand1_value, operand2_value, registers[0]);
}

void conditional_jump(const Instruction *instruction, FILE *file) {
    if (registers[0] == 1) {
        int jump_to = atoi(instruction->operand1); // 점프할 명령어 줄 번호
        printf("Conditional jump to instruction %d\n", jump_to);

        // 파일 포인터를 초기 위치로 이동
        rewind(file);

        char line[100];
        int current_line = 0;

        // 파일을 다시 읽으면서 점프할 줄까지 이동
        while (fgets(line, sizeof(line), file)) {
            current_line++;
            if (current_line == jump_to) {
                // 점프할 줄에 도달하면 파일 포인터를 해당 위치로 이동
                printf("Jumped to instruction %d\n", jump_to);
                return;
            }
        }

        // 점프할 줄이 파일에 없는 경우
        printf("Error: Jump target %d is out of range\n", jump_to);
    }
}

void jump(const Instruction *instruction, FILE *file) {
    int jump_to = atoi(instruction->operand1); // 점프할 명령어 줄 번호
    printf("Jump to instruction %d\n", jump_to);

    // 파일 포인터를 초기 위치로 이동
    rewind(file);

    char line[100];
    int current_line = 0;

    // 파일을 다시 읽으면서 점프할 줄까지 이동
    while (fgets(line, sizeof(line), file)) {
        current_line++;
        if (current_line == jump_to) {
            // 점프할 줄에 도달하면 파일 포인터를 해당 위치로 이동
            printf("Jumped to instruction %d\n", jump_to);
            return;
        }
    }

    // 점프할 줄이 파일에 없는 경우
    printf("Error: Jump target %d is out of range\n", jump_to);
}

void reset_registers() {
    for (int i = 0; i < Max_REGISTERS; i++) {
        registers[i] = 0; // 모든 레지스터를 0으로 초기화
    }
    printf("All registers have been reset to 0.\n");
}

void execute_instruction(const Instruction *instruction, FILE *file) {
    if (strcmp(instruction->opcode, "M") == 0) {
        update_register(instruction->operand1, instruction->operand2);
    }
    else if(strcmp(instruction->opcode, "LW") == 0){
        load_word(instruction);
    }
    else if(strcmp(instruction->opcode, "SW") == 0){
        store_word(instruction);
    }
    else if(strcmp(instruction->opcode, "C") == 0){
        compare_operand(instruction);
    }
    else if(strcmp(instruction->opcode, "B") == 0){
        conditional_jump(instruction, file);
    }
    else if(strcmp(instruction->opcode, "J") == 0){
        jump(instruction, file); // Jump 명령어 처리
    }
    else if(strcmp(instruction->opcode, "reset") == 0){
        reset_registers(); // reset 명령어 처리
    }
    else if(strcmp(instruction->opcode, "H") == 0){
        printf("Halt: Program execution completed.\n");
        exit(0); // 프로그램 종료
    }
    else {
        Simple_Math(instruction);
    }
}

void update_register(const char *dest, const char *src) {
    int dest_index = atoi(dest + 1); // "R1"에서 "1" 추출 (0-based 인덱스)
    int src_value = 0;

    if (src[0] == 'R') {
        // operand2_value가 레지스터인 경우
        int src_index = atoi(src + 1); // "R0"에서 "0" 추출 (0-based 인덱스)
        src_value = registers[src_index]; // 소스 레지스터의 값
    } else {
        // 소스가 값인 경우 (예: "0x4")
        src_value = strtol(src, NULL, 0); // 16진수를 정수로 변환
    }

    // 레지스터 이동 또는 값 설정
    registers[dest_index] = src_value;

    // 이동/설정 결과 로깅
    printf("레지스터 %s에서 %s로 값 %d 이동\n", src, dest, src_value);
}

void print_registers() {
    printf("현재 레지스터 상태:\n");
    for (int i = 0; i < Max_REGISTERS; i++) {
        printf("R%d: %d\n", i, registers[i]);
    }
}

int main(void){
    Instruction instruction;
    char line[100];

    FILE *file = fopen("C:\\Users\\ldj23\\Desktop\\computer science\\hw1\\gcd.txt", "r");
    if(file == NULL){
        perror("Unable to open the file");
        return 1;
    }

    while(fgets(line, sizeof(line), file)){
        if(sscanf(line, "%s %s %s", instruction.opcode, instruction.operand1, instruction.operand2) == 3){
            printf("opcode: %s, operand1: %s, operand2: %s\n", instruction.opcode, instruction.operand1, instruction.operand2);
            execute_instruction(&instruction, file);
            print_registers();
        }
    }

    fclose(file);
    return 0;
}