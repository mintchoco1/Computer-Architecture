#include "structure.h"

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