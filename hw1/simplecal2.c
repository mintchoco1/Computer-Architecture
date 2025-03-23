#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINES 100
#define REG_COUNT 10

int registers[REG_COUNT];
char* instructions[MAX_LINES];
int ip = 0;  // Instruction pointer
int line_count = 0;

// Function prototypes
void parse_operand(char* operand, int* value);
void execute_instruction(char* opcode, char* op1, char* op2);
void print_registers();
void handle_exception(const char* message);

int main() {
    FILE* file = fopen("input.txt", "r");
    if (!file) {
        perror("Error opening file");
        return 1;
    }

    // Read instructions
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        instructions[line_count] = strdup(line);
        line_count++;
        if (line_count >= MAX_LINES) break;
    }
    fclose(file);

    // Main execution loop
    while (ip < line_count) {
        char* line = instructions[ip];
        char* opcode = strtok(line, " \t\n");
        if (!opcode) {
            ip++;
            continue;
        }

        if (opcode[0] == 'H') break;

        char* op1 = strtok(NULL, " \t\n");
        char* op2 = strtok(NULL, " \t\n");
        
        execute_instruction(opcode, op1, op2);
        print_registers();
        ip++;
    }

    // Free allocated memory
    for (int i = 0; i < line_count; i++) {
        free(instructions[i]);
    }

    return 0;
}

void parse_operand(char* operand, int* value) {
    if (operand == NULL) {
        *value = 0;
        return;
    }
    
    if (strncmp(operand, "0x", 2) == 0) {
        *value = (int)strtol(operand + 2, NULL, 16);
    }
    else if (operand[0] == 'R') {
        int reg = atoi(operand + 1);
        if (reg < 0 || reg >= REG_COUNT) {
            handle_exception("Invalid register");
            return;
        }
        *value = registers[reg];
    }
    else {
        *value = atoi(operand);
    }
}

void execute_instruction(char* opcode, char* op1, char* op2) {
    int val1, val2, result;
    
    switch (opcode[0]) {
        case '+':
            parse_operand(op1, &val1);
            parse_operand(op2, &val2);
            registers[0] = val1 + val2;
            break;
            
        case '-':
            parse_operand(op1, &val1);
            parse_operand(op2, &val2);
            registers[0] = val1 - val2;
            break;
            
        case '*':
            parse_operand(op1, &val1);
            parse_operand(op2, &val2);
            registers[0] = val1 * val2;
            break;
            
        case '/':
            parse_operand(op1, &val1);
            parse_operand(op2, &val2);
            if (val2 == 0) handle_exception("Division by zero");
            else registers[0] = val1 / val2;
            break;
            
        case 'M':
            parse_operand(op2, &val2);
            if (op1[0] != 'R') handle_exception("Invalid register");
            int reg = atoi(op1 + 1);
            if (reg < 0 || reg >= REG_COUNT) {
                handle_exception("Invalid register");
                break;
            }
            registers[reg] = val2;
            break;
            
        case 'J':
            parse_operand(op1, &val1);
            ip = val1 - 1;  // Adjust for ip++ in main loop
            break;
            
        case 'C':
            parse_operand(op1, &val1);
            parse_operand(op2, &val2);
            registers[0] = (val1 >= val2) ? 0 : 1;
            break;
            
        case 'B':
            if (registers[0] == 1) {
                parse_operand(op1, &val1);
                ip = val1 - 1;  // Adjust for ip++ in main loop
            }
            break;
            
        default:
            handle_exception("Unknown opcode");
            break;
    }
}

void print_registers() {
    printf("Registers:\n");
    for (int i = 0; i < REG_COUNT; i++) {
        if (registers[i] != 0 || i == 0) {
            printf("R%d: %d\n", i, registers[i]);
        }
    }
    printf("----------------\n");
}

void handle_exception(const char* message) {
    fprintf(stderr, "Error: %s at line %d\n", message, ip + 1);
    exit(1);
}