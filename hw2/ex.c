#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MEM_SIZE (1024 * 1024) // 1MB memory
#define REG_COUNT 32
#define PC_START 0x00000000
#define PC_HALT 0xFFFFFFFF

// Instruction formats
typedef enum {
    R_TYPE,
    I_TYPE,
    J_TYPE
} InstructionType;

// Control signals structure
typedef struct {
    uint8_t RegDst;
    uint8_t Jump;
    uint8_t Branch;
    uint8_t MemRead;
    uint8_t MemtoReg;
    uint8_t ALUOp;
    uint8_t MemWrite;
    uint8_t ALUSrc;
    uint8_t RegWrite;
} ControlSignals;

// ALU control signals
typedef enum {
    ALU_ADD,    // 0000
    ALU_SUB,    // 0001
    ALU_AND,    // 0010
    ALU_OR,     // 0011
    ALU_XOR,    // 0100
    ALU_NOR,    // 0101
    ALU_SLT,    // 0110
    ALU_SLTU,   // 0111
    ALU_SLL,    // 1000
    ALU_SRL,    // 1001
    ALU_LUI     // 1010 (for LUI instruction)
} ALUControl;

// CPU state
typedef struct {
    uint32_t reg[REG_COUNT];    // General purpose registers
    uint8_t *mem;               // Memory
    uint32_t pc;                // Program counter
    uint32_t next_pc;           // Next program counter
    uint32_t instr;             // Current instruction
    ControlSignals ctrl;        // Control signals
    uint32_t alu_result;        // ALU result
    uint32_t mem_data;          // Data read from memory
    uint32_t write_data;        // Data to be written to register
    uint32_t write_reg;         // Register to write to
    uint8_t alu_zero;           // ALU zero flag
} CPUState;

// Function prototypes
void initialize_cpu(CPUState *cpu);
void fetch_instruction(CPUState *cpu);
void decode_instruction(CPUState *cpu);
void execute_instruction(CPUState *cpu);
void memory_access(CPUState *cpu);
void write_back(CPUState *cpu);
void update_pc(CPUState *cpu);
void print_state(CPUState *cpu);
void set_control_signals(CPUState *cpu);
ALUControl get_alu_control(CPUState *cpu);
void run_simulation(CPUState *cpu);

// Helper functions
uint32_t sign_extend(uint16_t imm);
uint32_t zero_extend(uint16_t imm);
uint32_t get_rs(CPUState *cpu);
uint32_t get_rt(CPUState *cpu);
uint32_t get_rd(CPUState *cpu);
uint16_t get_imm(CPUState *cpu);
uint32_t get_target(CPUState *cpu);

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <input.bin>\n", argv[0]);
        return 1;
    }

    CPUState cpu;
    initialize_cpu(&cpu);

    // Load binary file into memory
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        perror("Failed to open file");
        return 1;
    }

    fread(cpu.mem, 1, MEM_SIZE, file);
    fclose(file);

    run_simulation(&cpu);

    free(cpu.mem);
    return 0;
}

void initialize_cpu(CPUState *cpu) {
    cpu->mem = (uint8_t *)malloc(MEM_SIZE);
    memset(cpu->mem, 0, MEM_SIZE);
    memset(cpu->reg, 0, sizeof(cpu->reg));
    
    // Initialize special registers
    cpu->reg[29] = 0x1000000; // SP (r29)
    cpu->reg[31] = 0xFFFFFFFF; // RA (r31)
    
    cpu->pc = PC_START;
    cpu->next_pc = PC_START;
    cpu->instr = 0;
    memset(&cpu->ctrl, 0, sizeof(cpu->ctrl));
    cpu->alu_result = 0;
    cpu->mem_data = 0;
    cpu->write_data = 0;
    cpu->write_reg = 0;
    cpu->alu_zero = 0;
}

void fetch_instruction(CPUState *cpu) {
    // Read 4 bytes from memory at PC
    cpu->instr = *(uint32_t *)(cpu->mem + cpu->pc);
    
    // Convert from big-endian to little-endian if necessary
    cpu->instr = ((cpu->instr & 0xFF000000) >> 24) |
                 ((cpu->instr & 0x00FF0000) >> 8) |
                 ((cpu->instr & 0x0000FF00) << 8) |
                 ((cpu->instr & 0x000000FF) << 24);
    
    // Default next PC is PC + 4
    cpu->next_pc = cpu->pc + 4;
}

void decode_instruction(CPUState *cpu) {
    // Determine instruction type and set control signals
    set_control_signals(cpu);
    
    // Get register values
    uint32_t rs_val = get_rs(cpu);
    uint32_t rt_val = get_rt(cpu);
    
    // For R-type instructions, the second ALU operand is rt
    // For I-type instructions, it's the immediate value
    uint32_t alu_src2 = cpu->ctrl.ALUSrc ? sign_extend(get_imm(cpu)) : rt_val;
    
    // Perform ALU operation based on ALU control
    ALUControl alu_ctrl = get_alu_control(cpu);
    
    switch (alu_ctrl) {
        case ALU_ADD:
            cpu->alu_result = rs_val + alu_src2;
            break;
        case ALU_SUB:
            cpu->alu_result = rs_val - alu_src2;
            break;
        case ALU_AND:
            cpu->alu_result = rs_val & alu_src2;
            break;
        case ALU_OR:
            cpu->alu_result = rs_val | alu_src2;
            break;
        case ALU_XOR:
            cpu->alu_result = rs_val ^ alu_src2;
            break;
        case ALU_NOR:
            cpu->alu_result = ~(rs_val | alu_src2);
            break;
        case ALU_SLT:
            cpu->alu_result = ((int32_t)rs_val < (int32_t)alu_src2) ? 1 : 0;
            break;
        case ALU_SLTU:
            cpu->alu_result = (rs_val < alu_src2) ? 1 : 0;
            break;
        case ALU_SLL:
            cpu->alu_result = rt_val << (get_imm(cpu) & 0x1F);
            break;
        case ALU_SRL:
            cpu->alu_result = rt_val >> (get_imm(cpu) & 0x1F);
            break;
        case ALU_LUI:
            cpu->alu_result = get_imm(cpu) << 16;
            break;
    }
    
    // Set ALU zero flag for branch instructions
    cpu->alu_zero = (rs_val == rt_val);
    
    // Calculate branch target
    uint32_t branch_target = cpu->pc + 4 + (sign_extend(get_imm(cpu)) << 2);
    
    // Calculate jump target
    uint32_t jump_target = (cpu->pc & 0xF0000000) | ((cpu->instr & 0x03FFFFFF) << 2);
    
    // Update next PC based on control signals
    if (cpu->ctrl.Branch && cpu->alu_zero) {
        cpu->next_pc = branch_target;
    } else if (cpu->ctrl.Jump) {
        cpu->next_pc = jump_target;
    }
    
    // Determine which register to write to
    cpu->write_reg = cpu->ctrl.RegDst ? get_rd(cpu) : get_rt(cpu);
}

void execute_instruction(CPUState *cpu) {
    // Most execution is done in decode for single-cycle
    // This function is kept for symmetry with the pipeline stages
}

void memory_access(CPUState *cpu) {
    if (cpu->ctrl.MemRead) {
        // Load instruction
        uint32_t addr = cpu->alu_result;
        if (addr + 4 > MEM_SIZE) {
            printf("Memory access out of bounds: 0x%08X\n", addr);
            exit(1);
        }
        cpu->mem_data = *(uint32_t *)(cpu->mem + addr);
        
        // Convert from big-endian to little-endian if necessary
        cpu->mem_data = ((cpu->mem_data & 0xFF000000) >> 24) |
                        ((cpu->mem_data & 0x00FF0000) >> 8) |
                        ((cpu->mem_data & 0x0000FF00) << 8) |
                        ((cpu->mem_data & 0x000000FF) << 24);
    } else if (cpu->ctrl.MemWrite) {
        // Store instruction
        uint32_t addr = cpu->alu_result;
        if (addr + 4 > MEM_SIZE) {
            printf("Memory access out of bounds: 0x%08X\n", addr);
            exit(1);
        }
        
        uint32_t data = get_rt(cpu);
        // Convert to big-endian if necessary
        data = ((data & 0xFF000000) >> 24) |
               ((data & 0x00FF0000) >> 8) |
               ((data & 0x0000FF00) << 8) |
               ((data & 0x000000FF) << 24);
        
        *(uint32_t *)(cpu->mem + addr) = data;
    }
    
    // Determine data to write back to register
    cpu->write_data = cpu->ctrl.MemtoReg ? cpu->mem_data : cpu->alu_result;
    
    // Special case for JAL: write return address to $ra
    if ((cpu->instr >> 26) == 0x03) { // JAL
        cpu->write_data = cpu->pc + 4;
        cpu->write_reg = 31; // $ra
    }
}

void write_back(CPUState *cpu) {
    if (cpu->ctrl.RegWrite && cpu->write_reg != 0) { // $zero is read-only
        cpu->reg[cpu->write_reg] = cpu->write_data;
    }
}

void update_pc(CPUState *cpu) {
    cpu->pc = cpu->next_pc;
}

void print_state(CPUState *cpu) {
    printf("PC: 0x%08X\n", cpu->pc);
    printf("Instruction: 0x%08X\n", cpu->instr);
    
    printf("Registers:\n");
    for (int i = 0; i < REG_COUNT; i++) {
        if (i % 4 == 0) printf("\n");
        printf("R%d: 0x%08X\t", i, cpu->reg[i]);
    }
    printf("\n");
    
    printf("ALU Result: 0x%08X\n", cpu->alu_result);
    printf("Memory Data: 0x%08X\n", cpu->mem_data);
    printf("Write Data: 0x%08X to R%d\n", cpu->write_data, cpu->write_reg);
    printf("----------------------------------------\n");
}

void set_control_signals(CPUState *cpu) {
    uint8_t opcode = cpu->instr >> 26;
    uint8_t funct = cpu->instr & 0x3F;
    
    // Default values
    cpu->ctrl.RegDst = 0;
    cpu->ctrl.Jump = 0;
    cpu->ctrl.Branch = 0;
    cpu->ctrl.MemRead = 0;
    cpu->ctrl.MemtoReg = 0;
    cpu->ctrl.ALUOp = 0;
    cpu->ctrl.MemWrite = 0;
    cpu->ctrl.ALUSrc = 0;
    cpu->ctrl.RegWrite = 0;
    
    // R-type instructions
    if (opcode == 0x00) {
        cpu->ctrl.RegDst = 1;
        cpu->ctrl.RegWrite = 1;
        cpu->ctrl.ALUOp = 2; // ALUOp = 2 means look at funct field
        
        // Special cases for JR and JALR
        if (funct == 0x08) { // JR
            cpu->ctrl.RegWrite = 0;
            cpu->ctrl.Jump = 1;
            cpu->next_pc = get_rs(cpu);
        } else if (funct == 0x09) { // JALR
            cpu->ctrl.RegWrite = 1;
            cpu->ctrl.Jump = 1;
            cpu->write_reg = get_rd(cpu);
            cpu->write_data = cpu->pc + 4;
            cpu->next_pc = get_rs(cpu);
        }
    }
    // I-type instructions
    else {
        switch (opcode) {
            case 0x08: // ADDI
            case 0x09: // ADDIU
            case 0x0C: // ANDI
            case 0x0D: // ORI
            case 0x0A: // SLTI
            case 0x0B: // SLTIU
            case 0x0F: // LUI
                cpu->ctrl.ALUSrc = 1;
                cpu->ctrl.RegWrite = 1;
                cpu->ctrl.ALUOp = 1; // ALUOp = 1 means look at opcode
                break;
                
            case 0x04: // BEQ
            case 0x05: // BNE
                cpu->ctrl.Branch = 1;
                cpu->ctrl.ALUOp = 1;
                break;
                
            case 0x23: // LW
                cpu->ctrl.ALUSrc = 1;
                cpu->ctrl.MemtoReg = 1;
                cpu->ctrl.RegWrite = 1;
                cpu->ctrl.MemRead = 1;
                cpu->ctrl.ALUOp = 0; // ALUOp = 0 means add
                break;
                
            case 0x2B: // SW
                cpu->ctrl.ALUSrc = 1;
                cpu->ctrl.MemWrite = 1;
                cpu->ctrl.ALUOp = 0; // ALUOp = 0 means add
                break;
                
            case 0x02: // J
            case 0x03: // JAL
                cpu->ctrl.Jump = 1;
                if (opcode == 0x03) { // JAL
                    cpu->ctrl.RegWrite = 1;
                    cpu->write_reg = 31; // $ra
                }
                break;
                
            default:
                printf("Unknown opcode: 0x%02X\n", opcode);
                exit(1);
        }
    }
}

ALUControl get_alu_control(CPUState *cpu) {
    uint8_t opcode = cpu->instr >> 26;
    uint8_t funct = cpu->instr & 0x3F;
    
    if (cpu->ctrl.ALUOp == 0) { // For LW/SW
        return ALU_ADD;
    } else if (cpu->ctrl.ALUOp == 1) { // For I-type instructions
        switch (opcode) {
            case 0x08: // ADDI
            case 0x09: // ADDIU
            case 0x23: // LW
            case 0x2B: // SW
                return ALU_ADD;
            case 0x0C: // ANDI
                return ALU_AND;
            case 0x0D: // ORI
                return ALU_OR;
            case 0x0A: // SLTI
                return ALU_SLT;
            case 0x0B: // SLTIU
                return ALU_SLTU;
            case 0x0F: // LUI
                return ALU_LUI;
            case 0x04: // BEQ
            case 0x05: // BNE
                return ALU_SUB; // For comparison
            default:
                return ALU_ADD;
        }
    } else if (cpu->ctrl.ALUOp == 2) { // For R-type instructions
        switch (funct) {
            case 0x20: // ADD
                return ALU_ADD;
            case 0x21: // ADDU
                return ALU_ADD;
            case 0x22: // SUB
                return ALU_SUB;
            case 0x23: // SUBU
                return ALU_SUB;
            case 0x24: // AND
                return ALU_AND;
            case 0x25: // OR
                return ALU_OR;
            case 0x27: // NOR
                return ALU_NOR;
            case 0x2A: // SLT
                return ALU_SLT;
            case 0x2B: // SLTU
                return ALU_SLTU;
            case 0x00: // SLL
                return ALU_SLL;
            case 0x02: // SRL
                return ALU_SRL;
            default:
                printf("Unknown funct code: 0x%02X\n", funct);
                exit(1);
        }
    }
    
    return ALU_ADD;
}

void run_simulation(CPUState *cpu) {
    while (cpu->pc != PC_HALT) {
        fetch_instruction(cpu);
        decode_instruction(cpu);
        execute_instruction(cpu);
        memory_access(cpu);
        write_back(cpu);
        
        print_state(cpu);
        
        update_pc(cpu);
        
        // Check for infinite loop
        if (cpu->pc == 0) {
            printf("Potential infinite loop detected at PC=0\n");
            break;
        }
    }
    
    printf("Execution completed. Final register values:\n");
    for (int i = 0; i < REG_COUNT; i++) {
        printf("R%d: 0x%08X\n", i, cpu->reg[i]);
    }
    printf("Return value (R2): 0x%08X\n", cpu->reg[2]);
}

// Helper functions
uint32_t sign_extend(uint16_t imm) {
    return (int32_t)(int16_t)imm;
}

uint32_t zero_extend(uint16_t imm) {
    return (uint32_t)imm;
}

uint32_t get_rs(CPUState *cpu) {
    return cpu->reg[(cpu->instr >> 21) & 0x1F];
}

uint32_t get_rt(CPUState *cpu) {
    return cpu->reg[(cpu->instr >> 16) & 0x1F];
}

uint32_t get_rd(CPUState *cpu) {
    return (cpu->instr >> 11) & 0x1F;
}

uint16_t get_imm(CPUState *cpu) {
    return cpu->instr & 0xFFFF;
}

uint32_t get_target(CPUState *cpu) {
    return cpu->instr & 0x03FFFFFF;
}