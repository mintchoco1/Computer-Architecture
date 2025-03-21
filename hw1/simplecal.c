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
        int reg_index = atoi(operand + 1) - 1;
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
    int reg_index = atoi(instruction->operand1 + 1) - 1;
    int address = get_operand_value(instruction->operand2);
    registers[reg_index] = address; // 간단히 주소 값을 레지스터에 저장
    printf("Loaded value from address 0x%x into R%d\n", address, reg_index + 1);
}

void store_word(const Instruction *instruction){
    int reg_index = atoi(instruction->operand1 + 1) - 1;
    int address = get_operand_value(instruction->operand2);
    printf("Stored value %d from R%d into address 0x%x\n", registers[reg_index], reg_index + 1, address);
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

void conditional_jump(const Instruction *instruction){
    if (registers[0] == 1) {
        int jump_to = atoi(instruction->operand1);
        printf("Conditional jump to instruction %d\n", jump_to);
        // 여기서는 단순히 출력만 하고 실제로 점프를 구현하지는 않음
    }
}

void execute_instruction(const Instruction *instruction) {
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
        conditional_jump(instruction);
    }
    else if(strcmp(instruction->opcode, "J") == 0){
        int jump_to = atoi(instruction->operand1);
        printf("Jump to instruction %d\n", jump_to);
        // 실제로는 파일 포인터를 이동하여 해당 명령어로 점프해야 함
    }
    else {
        Simple_Math(instruction);
    }
}

void update_register(const char *dest, const char *src) {
    int dest_index = atoi(dest + 1) - 1; // "R1"에서 "1" 추출 후 인덱스로 변환 (0-based)
    int src_value = 0;

    if (src[0] == 'R') {
        // operand2_value가 레지스터인 경우
        int src_index = atoi(src + 1); // "R0"에서 "0" 추출 후 인덱스로 변환
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

    FILE *file = fopen("C:\\Users\\ldj23\\Desktop\\computer science\\hw1\\input.txt", "r");
    if(file == NULL){
        perror("Unable to open the file");
        return 1;
    }

    while(fgets(line, sizeof(line), file)){
        if(sscanf(line, "%s %s %s", instruction.opcode, instruction.operand1, instruction.operand2) == 3){
            printf("opcode: %s, operand1: %s, operand2: %s\n", instruction.opcode, instruction.operand1, instruction.operand2);
            execute_instruction(&instruction);
            print_registers();
        }
    }
    fclose(file);
    return 0;
}