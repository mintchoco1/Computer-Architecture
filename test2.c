#include <stdio.h>
#include <stdint.h>

#define MEMORY_SIZE 0x1000000

uint8_t memory[MEMORY_SIZE];

// Counters for statistics
int instruction_count = 0;
int rtype_count = 0;
int itype_count = 0;
int jtype_count = 0;
int memory_count = 0;
int branch_count = 0;

// For multiplication instructions
uint64_t high_word = 0;
uint64_t low_word = 0;

// Register file structure
typedef struct {
    uint32_t regs[32];
    int program_counter;
} RegisterFile;

// Control signals structure
typedef struct {
    int read_memory;
    int write_result_from_memory;
    int write_memory;
    int write_register;
    int alu_operation;
    int skip_execution;
    int skip_memory;
} ControlSignals;

// Function prototypes
void initialize_control(ControlSignals* control);
void setup_execution_stage_control(ControlSignals* control, uint32_t* decoded_instruction);
void setup_memory_stage_control(ControlSignals* control, uint32_t* decoded_instruction);
void setup_writeback_stage_control(ControlSignals* control, uint32_t* decoded_instruction);
uint32_t instruction_fetch(RegisterFile* registers, uint8_t* memory);
void instruction_decode(RegisterFile* registers, ControlSignals* control, uint32_t instruction, uint32_t* id_to_ex, uint32_t* address_info);
uint32_t alu_operation(ControlSignals* control, uint32_t operand1, uint32_t operand2);
void instruction_execute(RegisterFile* registers, ControlSignals* control, uint32_t* id_to_ex, uint32_t* ex_to_mem, uint32_t* address_info);
void memory_stage(ControlSignals* control, uint32_t* ex_to_mem, uint32_t* mem_to_wb);
void writeback_stage(RegisterFile* registers, ControlSignals* control, uint32_t* mem_to_wb);
void clear_pipeline_registers(uint32_t* instruction, uint32_t* address_info, uint32_t* id_to_ex, uint32_t* ex_to_mem, uint32_t* mem_to_wb);
void run_processor(RegisterFile* registers, uint8_t* memory);
void setup_registers(RegisterFile* registers);
void extend_immediate_values(uint32_t* id_to_ex, uint32_t* address_info);

void initialize_control(ControlSignals* control) {
    control->read_memory = 0;
    control->write_result_from_memory = 0;
    control->write_memory = 0;
    control->write_register = 1;
    control->alu_operation = -1;
    control->skip_execution = 0;
    control->skip_memory = 0;
}

// Setup control signals for execution stage - handles R-type operations
void setup_rtype_execution(ControlSignals* control, uint32_t function_code) {
    switch (function_code) {
        case 0x20: // add
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
            control->alu_operation = 0b0111;
            break;
        case 0x2b: // sltu
            control->alu_operation = 0b0111;
            break;
        case 0x23: // subu
            control->alu_operation = 0b0110;
            break;
        case 0x21: // addu
            control->alu_operation = 0b0010;
            break;
    }
}

// Setup control signals for execution stage - handles I-type operations
void setup_itype_execution(ControlSignals* control, uint32_t opcode) {
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

// Setup control signals for execution stage
void setup_execution_stage_control(ControlSignals* control, uint32_t* decoded_instruction) {
    if (decoded_instruction[0] == 0) { // R-type
        setup_rtype_execution(control, decoded_instruction[2]);
    } else { // I-type or J-type
        setup_itype_execution(control, decoded_instruction[0]);
    }
}

// Setup control signals for memory stage
void setup_memory_stage_control(ControlSignals* control, uint32_t* decoded_instruction) {
    // Set skip_memory for certain instruction types
    if (decoded_instruction[1] == 1 || decoded_instruction[1] == 3 || 
        decoded_instruction[1] == 4 || decoded_instruction[1] == 5) {
        control->skip_memory = 1;
    }
    
    // Set memory read/write signals
    if (decoded_instruction[0] == 35) { // lw
        control->read_memory = 1;
        control->write_memory = 0;
    } else if (decoded_instruction[0] == 43) { // sw
        control->read_memory = 0;
        control->write_memory = 1;
    }
}

// Setup control signals for writeback stage
void setup_writeback_stage_control(ControlSignals* control, uint32_t* decoded_instruction) {
    // Set register write signal
    if (decoded_instruction[1] == 1 || decoded_instruction[1] == 3 || 
        decoded_instruction[1] == 4 || decoded_instruction[1] == 5) {
        control->write_register = 0;
    }
    
    if (decoded_instruction[0] == 43) { // sw
        control->write_register = 0;
    }
    
    // Set memory-to-register signal
    if (decoded_instruction[0] == 0) { // R-type
        control->write_result_from_memory = 0;
    } else { // I-type
        if (decoded_instruction[0] == 35) { // lw
            control->write_result_from_memory = 1;
        }
    }
}

// Fetch instruction from memory
uint32_t instruction_fetch(RegisterFile* registers, uint8_t* memory) {
    printf("\t[Instruction Fetch] ");
    uint32_t instruction = 0;
    
    for (int i = 0; i < 4; i++) {
        instruction = instruction << 8;
        instruction |= memory[registers->program_counter + (3 - i)];
    }
    
    registers->program_counter += 4;
    printf(" 0x%08x (PC=0x%08x)\n", instruction, registers->program_counter);
    return instruction;
}

// Extract fields from R-type instruction
void decode_rtype(uint32_t instruction, uint32_t* parts) {
    parts[1] = (instruction >> 21) & 0x1f; // rs
    parts[2] = (instruction >> 16) & 0x1f; // rt
    parts[3] = (instruction >> 11) & 0x1f; // rd
    parts[4] = instruction & 0x3f;         // funct
    parts[5] = (instruction >> 6) & 0x1f;  // shamt
}

// Extract fields from I-type instruction
void decode_itype(uint32_t instruction, uint32_t* parts) {
    parts[1] = (instruction >> 21) & 0x1f; // rs
    parts[2] = (instruction >> 16) & 0x1f; // rt
    parts[3] = instruction & 0xffff;       // immediate
}

// Extract fields from J-type instruction
void decode_jtype(uint32_t instruction, uint32_t* parts) {
    parts[1] = instruction & 0x3ffffff;    // target address
}

// Decode R-type instruction for display
void display_rtype(uint32_t* parts) {
    printf("R, Inst: ");
    
    if (parts[4] == 0x08) {
        printf("jr r%d", parts[1]);
    } else if (parts[4] == 0x20) {
        printf("add r%d r%d r%d", parts[3], parts[2], parts[1]);
    } else if (parts[4] == 0x22) {
        printf("sub r%d r%d r%d", parts[3], parts[2], parts[1]);
    } else if (parts[4] == 0x24) {
        printf("and r%d r%d r%d", parts[3], parts[2], parts[1]);
    } else if (parts[4] == 0x25) {
        printf("or r%d r%d r%d", parts[3], parts[2], parts[1]);
    } else if (parts[4] == 0x27) {
        printf("nor r%d r%d r%d", parts[3], parts[2], parts[1]);
    } else if (parts[4] == 0x00) {
        printf("sll r%d r%d %d", parts[3], parts[2], parts[5]);
    } else if (parts[4] == 0x02) {
        printf("srl r%d r%d r%d", parts[3], parts[2], parts[5]);
    } else if (parts[4] == 0x2a) {
        printf("slt r%d r%d r%d", parts[3], parts[2], parts[1]);
    } else if (parts[4] == 0x2b) {
        printf("sltu r%d r%d r%d", parts[3], parts[2], parts[1]);
    } else if (parts[4] == 0x23) {
        printf("subu r%d r%d r%d", parts[3], parts[2], parts[1]);
    } else if (parts[4] == 0x21) {
        printf("addu r%d r%d r%d", parts[3], parts[2], parts[1]);
    } else if (parts[4] == 0x18) {
        printf("mult r%d r%d", parts[2], parts[1]);
    }
}

// Decode I-type instruction for display
void display_itype(uint32_t* parts) {
    printf("I, Inst: ");
    
    if (parts[0] == 4) {
        printf("beq r%d r%d %x", parts[2], parts[1], parts[3]);
    } else if (parts[0] == 5) {
        printf("bne r%d r%d %x", parts[2], parts[1], parts[3]);
    } else if (parts[0] == 8) {
        printf("addi r%d r%d %d", parts[2], parts[1], parts[3]);
    } else if (parts[0] == 9) {
        printf("addiu r%d r%d %d", parts[2], parts[1], parts[3]);
    } else if (parts[0] == 10) {
        printf("slti r%d r%d %d", parts[2], parts[1], parts[3]);
    } else if (parts[0] == 11) {
        printf("sltiu r%d r%d %d", parts[2], parts[1], parts[3]);
    } else if (parts[0] == 12) {
        printf("andi r%d r%d %d", parts[2], parts[1], parts[3]);
    } else if (parts[0] == 13) {
        printf("ori r%d r%d %d", parts[2], parts[1], parts[3]);
    } else if (parts[0] == 35) {
        printf("lw r%d %d(r%d)", parts[2], parts[3], parts[1]);
    } else if (parts[0] == 43) {
        printf("sw r%d %d(r%d)", parts[2], parts[3], parts[1]);
    } else if (parts[0] == 0xf) {
        printf("lui r%d %d", parts[2], parts[3]);
    }
}

// Decode J-type instruction for display
void display_jtype(uint32_t* parts) {
    printf("J, Inst: ");
    
    if (parts[0] == 2) {
        printf("j 0x%x", parts[1]);
    } else if (parts[0] == 3) {
        printf("jal 0x%x", parts[1]);
    }
}

// Handle sign extension for immediate values
void extend_immediate_values(uint32_t* id_to_ex, uint32_t* address_info) {
    if (id_to_ex[1] == 2) { // I-type immediate
        if (id_to_ex[0] == 0xc || id_to_ex[0] == 0xd || (id_to_ex[5] >> 15) == 0) {
            // Zero extend for logical operations (andi, ori)
            id_to_ex[5] = (id_to_ex[5] & 0x0000ffff);
        } else if ((id_to_ex[5] >> 15) == 1) { // Sign extend for others
            id_to_ex[5] = (id_to_ex[5] | 0xffff0000);
        }
    } else if (id_to_ex[1] == 1) { // J-type address
        address_info[0] <<= 2; // Word alignment
    } else if (id_to_ex[1] == 4) { // Branch address
        if ((id_to_ex[5] >> 15) == 1) {
            id_to_ex[5] = ((id_to_ex[5] << 2) | 0xfffc0000); // Sign extended
        } else if ((id_to_ex[5] >> 15) == 0) {
            id_to_ex[5] = ((id_to_ex[5] << 2) & 0x3ffc); // Zero extended
        }
    }
    
    // Store PC+4 value for branch and jump calculations
    address_info[1] = id_to_ex[7]; // Previously stored PC+4 value
}

// Decode instruction
void instruction_decode(RegisterFile* registers, ControlSignals* control, uint32_t instruction, uint32_t* id_to_ex, uint32_t* address_info) {
    printf("\t[Instruction Decode] ");
    instruction_count++;
    
    uint32_t instruction_parts[6] = { 0, };
    initialize_control(control);
    
    // Extract opcode
    instruction_parts[0] = instruction >> 26; // opcode [31-26]
    
    // Extract fields based on instruction type
    if (instruction_parts[0] == 2 || instruction_parts[0] == 3) {
        decode_jtype(instruction, instruction_parts);
    } else if (instruction_parts[0] == 0) {
        decode_rtype(instruction, instruction_parts);
    } else {
        decode_itype(instruction, instruction_parts);
    }

    printf("Type: ");
    if (instruction_parts[0] == 0) { // R-type
        id_to_ex[1] = 0;
        display_rtype(instruction_parts);
        rtype_count++;
        
        // Special case for 'jr' instruction
        if (instruction_parts[4] == 0x08) {
            id_to_ex[1] = 3; // Mark as jr
        }
    } else if (instruction_parts[0] == 2 || instruction_parts[0] == 3) { // J-type
        id_to_ex[1] = 1;
        display_jtype(instruction_parts);
        jtype_count++;
    } else { // I-type
        id_to_ex[1] = 2;
        
        if (instruction_parts[0] == 4 || instruction_parts[0] == 5) { // beq bne
            id_to_ex[1] = 4;
        }
        
        display_itype(instruction_parts);
        itype_count++;
    }
    
    printf("\n\t    opcode: 0x%x", instruction_parts[0]);
    
    // Display additional instruction information based on type
    if (instruction_parts[0] == 0) { // R-type
        if (instruction_parts[4] != 0x08 && instruction_parts[4] != 0x12 && instruction_parts[4] != 0x18) {
            printf(", rs: %d (0x%x), rt: %d (0x%x), rd: %d (0x%x)", 
                   instruction_parts[1], registers->regs[instruction_parts[1]], 
                   instruction_parts[2], registers->regs[instruction_parts[2]], 
                   instruction_parts[3], registers->regs[instruction_parts[3]]);
                   
            if (instruction_parts[4] == 0 || instruction_parts[4] == 2) {
                printf(", shmat: %d", instruction_parts[5]);
            }
        } else if (instruction_parts[4] == 0x08) { // jr
            printf(", rs: %d (0x%x)", instruction_parts[1], registers->regs[instruction_parts[1]]);
        } else if (instruction_parts[4] == 0x12) { // mflo
            printf(", rd: %d (0x%x)", instruction_parts[3], registers->regs[instruction_parts[3]]);
        } else if (instruction_parts[4] == 0x18) { // mult
            printf(", rs: %d (0x%x), rt: %d (0x%x)", 
                   instruction_parts[1], registers->regs[instruction_parts[1]], 
                   instruction_parts[2], registers->regs[instruction_parts[2]]);
        }
        
        printf(", funct: 0x%x", instruction_parts[4]);
    } else if (instruction_parts[0] == 2 || instruction_parts[0] == 3) { // J-type
        printf(", imm: %d", instruction_parts[1]);
    } else if (instruction_parts[0] == 0xf) {
        printf(", rt: %d (0x%x)", instruction_parts[2], registers->regs[instruction_parts[2]]);
    } else {
        printf(", rs: %d (0x%x), rt: %d (0x%x), imm: %d", 
               instruction_parts[1], registers->regs[instruction_parts[1]], 
               instruction_parts[2], registers->regs[instruction_parts[2]], 
               instruction_parts[3]);
    }
    
    printf("\n");

    // Calculate control signals for display
    int reg_dst, reg_write, alu_src, pc_src, mem_read, mem_write, mem_to_reg, alu_op;
    reg_dst = (instruction_parts[0] == 0) ? 1 : 0;
    reg_write = ((instruction_parts[0] == 0) || (instruction_parts[0] == 35)) ? 1 : 0;
    alu_src = ((instruction_parts[0] == 43) || (instruction_parts[0] == 35)) ? 1 : 0;
    pc_src = ((instruction_parts[0] >= 2) && (instruction_parts[0] <= 5)) ? 1 : 0;
    mem_read = (instruction_parts[0] == 35) ? 1 : 0;
    mem_write = (instruction_parts[0] == 43) ? 1 : 0;
    mem_to_reg = ((instruction_parts[0] == 0xf) || (instruction_parts[0] == 35)) ? 1 : 0;
    
    alu_op = 0;
    if ((instruction_parts[0] == 43) || (instruction_parts[0] == 35)) { // lw sw
        alu_op = 0;
    } else if ((instruction_parts[0] == 4) || (instruction_parts[0] == 5)) { // branch
        alu_op = 0b01;
    } else if (instruction_parts[0] == 0) { // rtype
        alu_op = 0b10;
    }
    
    printf("\t    RegDst: %d, RegWrite: %d, ALUSrc: %d, PCSrc: %d, MemRead: %d, MemWrite: %d, MemtoReg: %d, ALUOp: %d\n", 
           reg_dst, reg_write, alu_src, pc_src, mem_read, mem_write, mem_to_reg, alu_op);

    // Prepare data for execution stage based on instruction type
    id_to_ex[0] = instruction_parts[0]; // opcode
    id_to_ex[7] = registers->program_counter; // PC+4 for branch/jump
    
    if (id_to_ex[1] == 0) { // R-type
        id_to_ex[2] = instruction_parts[4]; // function code
        id_to_ex[3] = instruction_parts[1]; // rs
        id_to_ex[4] = instruction_parts[2]; // rt
        id_to_ex[6] = instruction_parts[3]; // rd (write register)
        
        if (id_to_ex[2] == 0x0 || id_to_ex[2] == 0x2) {
            id_to_ex[7] = instruction_parts[5]; // shift amount
        }
    } else if (id_to_ex[1] == 1) { // J-type
        address_info[0] = instruction_parts[1]; // jump address
    } else if (id_to_ex[1] == 2) { // I-type
        id_to_ex[3] = instruction_parts[1]; // rs
        id_to_ex[5] = instruction_parts[3]; // immediate
        id_to_ex[6] = instruction_parts[2]; // rt (write register)
        
        if (id_to_ex[0] == 43) { // sw
            id_to_ex[4] = instruction_parts[2]; // rt (source register)
        }
    } else if (id_to_ex[1] == 3) { // jr
        id_to_ex[3] = instruction_parts[1]; // rs
    } else { // branch
        id_to_ex[3] = instruction_parts[1]; // rs
        id_to_ex[4] = instruction_parts[2]; // rt
        id_to_ex[5] = instruction_parts[3]; // immediate
        branch_count++;
    }

    // Sign extension for immediates
    extend_immediate_values(id_to_ex, address_info);
}

// Perform ALU operation
uint32_t alu_operation(ControlSignals* control, uint32_t operand1, uint32_t operand2) {
    uint32_t result = 0;
    
    switch (control->alu_operation) {
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

// Handle jump instructions
void process_jumps(RegisterFile* registers, uint32_t* id_to_ex, uint32_t* address_info) {
    if (id_to_ex[1] == 1) { // J-type
        if (id_to_ex[0] == 3) { // jal
            registers->regs[31] = id_to_ex[7];  // ra = PC+4
            registers->program_counter = address_info[0]; // PC = jump target
        } else if (id_to_ex[0] == 2) { // j
            registers->program_counter = address_info[0]; // PC = jump target
        }
    } else if (id_to_ex[1] == 3) { // jr
        registers->program_counter = registers->regs[id_to_ex[3]]; // PC = rs
    }
}

// Handle branch instructions
void process_branches(RegisterFile* registers, ControlSignals* control, uint32_t* id_to_ex, uint32_t* address_info) {
    uint32_t alu_result;
    address_info[2] = id_to_ex[5]; // Branch offset
    
    if (id_to_ex[0] == 4) { // beq
        alu_result = alu_operation(control, registers->regs[id_to_ex[3]], registers->regs[id_to_ex[4]]);
        if (alu_result == 0) { // Equal
            registers->program_counter = address_info[1] + address_info[2]; // PC = PC+4 + offset
            printf("%d\n", (alu_result == 0));
        } else {
            registers->program_counter = address_info[1]; // PC = PC+4 (no branch)
            printf("%d\n", (alu_result == 0));
        }
    } else if (id_to_ex[0] == 5) { // bne
        alu_result = alu_operation(control, registers->regs[id_to_ex[3]], registers->regs[id_to_ex[4]]);
        if (alu_result != 0) { // Not equal
            registers->program_counter = address_info[1] + address_info[2]; // PC = PC+4 + offset
            printf("%d\n", (alu_result != 0));
        } else {
            registers->program_counter = address_info[1]; // PC = PC+4 (no branch)
            printf("%d\n", (alu_result != 0));
        }
    }
}

// Execute stage of pipeline
void instruction_execute(RegisterFile* registers, ControlSignals* control, uint32_t* id_to_ex, uint32_t* ex_to_mem, uint32_t* address_info) {
    printf("\t[Execute] ");
    
    // Pass along instruction info to next stage
    ex_to_mem[0] = id_to_ex[0]; // opcode
    ex_to_mem[1] = id_to_ex[1]; // instruction type
    
    // Handle jumps first, as they update PC directly
    if (id_to_ex[1] == 1 || id_to_ex[1] == 3) { // Jump instructions
        process_jumps(registers, id_to_ex, address_info);
        printf("Pass\n");
        return;
    }

    // Special case for lui
    if (id_to_ex[0] == 0xf) { // lui
        registers->regs[id_to_ex[6]] = (id_to_ex[5] << 16) & 0xffff0000;
        printf("Pass\n");
        return;
    }

    // Special case for mflo
    if (id_to_ex[1] == 0 && id_to_ex[2] == 0x12) { // mflo
        registers->regs[id_to_ex[6]] = low_word;
        ex_to_mem[1] = 5; // Mark as special case
        printf("Pass\n");
        return;
    }

    // Setup control signals for ALU
    initialize_control(control);
    setup_execution_stage_control(control, id_to_ex);

    // Skip execution if needed
    if (control->skip_execution == 1) {
        ex_to_mem[4] = id_to_ex[6]; // Pass register destination
        return;
    }

    // Handle branch instructions
    if (id_to_ex[1] == 4) { // beq/bne
        process_branches(registers, control, id_to_ex, address_info);
        return;
    }

    printf("ALU = ");
    
    // Execute based on instruction type
    if (id_to_ex[1] == 0) { // R-type
        if (id_to_ex[2] == 0x00 || id_to_ex[2] == 0x02) { // sll, srl
            ex_to_mem[2] = alu_operation(control, registers->regs[id_to_ex[4]], id_to_ex[7]);
        } else {
            ex_to_mem[2] = alu_operation(control, registers->regs[id_to_ex[3]], registers->regs[id_to_ex[4]]);
        }
        ex_to_mem[4] = id_to_ex[6]; // Destination register
        ex_to_mem[3] = id_to_ex[4]; // Source register (for store)
    } else if (id_to_ex[1] == 2) { // I-type
        ex_to_mem[2] = alu_operation(control, registers->regs[id_to_ex[3]], id_to_ex[5]);
        ex_to_mem[4] = id_to_ex[6]; // Destination register
        if (id_to_ex[0] == 43) { // sw
            ex_to_mem[3] = registers->regs[id_to_ex[4]]; // Store value
        }
    }
    
    printf("0x%x\n", ex_to_mem[2]);
}

// Read data from memory
uint32_t read_from_memory(uint32_t address) {
    uint32_t data = 0;
    for (int j = 3; j >= 0; j--) {
        data <<= 8;
        data |= memory[address + j];
    }
    return data;
}

// Write data to memory
void write_to_memory(uint32_t address, uint32_t data) {
    for (int i = 0; i < 4; i++) {
        memory[address + i] = (data >> (8 * i)) & 0xFF;
    }
}

// Memory access stage of pipeline
void memory_stage(ControlSignals* control, uint32_t* ex_to_mem, uint32_t* mem_to_wb) {
    printf("\t[Memory Access] ");
    
    // Pass instruction info to writeback stage
    mem_to_wb[0] = ex_to_mem[0]; // opcode
    mem_to_wb[1] = ex_to_mem[1]; // instruction type
    
    // Setup control signals for memory access
    initialize_control(control);
    setup_memory_stage_control(control, ex_to_mem);

    // Skip memory access if needed (for branches, jumps, etc.)
    if (control->skip_memory) {
        if (ex_to_mem[1] == 0) { // R-type
            mem_to_wb[4] = ex_to_mem[4]; // Register destination
        }
        printf("Pass\n");
        return;
    }

    // Handle memory operations
    if (control->read_memory) { // lw
        printf("Load, Address: 0x%x", ex_to_mem[2]);
        memory_count++;
        
        // Read 4 bytes from memory
        mem_to_wb[2] = read_from_memory(ex_to_mem[2]);
        
        mem_to_wb[4] = ex_to_mem[4]; // Register destination
        printf(", Value: 0x%x", mem_to_wb[2]);
    } else if (control->write_memory) { // sw
        printf("Store, Address: 0x%x, Value: 0x%x", ex_to_mem[2], ex_to_mem[3]);
        memory_count++;
        
        // Write 4 bytes to memory
        write_to_memory(ex_to_mem[2], ex_to_mem[3]);
    } else {
        printf("Pass");
        mem_to_wb[3] = ex_to_mem[2]; // ALU result
        mem_to_wb[4] = ex_to_mem[4]; // Register destination
    }
    
    printf("\n");
}

// Writeback stage of pipeline
void writeback_stage(RegisterFile* registers, ControlSignals* control, uint32_t* mem_to_wb) {
    printf("\t[Write Back] newPC: 0x%x", registers->program_counter);
    
    // Setup control signals for writeback
    initialize_control(control);
    setup_writeback_stage_control(control, mem_to_wb);
    
    // Skip writeback if needed (for branches, jumps, stores)
    if (control->write_register == 0) {
        printf("\n");
        return;
    }
    
    // Determine value to write to register
    if (control->write_result_from_memory) { // lw
        registers->regs[mem_to_wb[4]] = mem_to_wb[2]; // From memory
    } else { // R-type or I-type (not lw)
        registers->regs[mem_to_wb[4]] = mem_to_wb[3]; // From ALU
    }
    
    printf("\n");
}

// Clear pipeline registers for next cycle
void clear_pipeline_registers(uint32_t* instruction, uint32_t* address_info, uint32_t* id_to_ex, uint32_t* ex_to_mem, uint32_t* mem_to_wb) {
    *instruction = 0;
    
    // Clear address info registers (PC, jump address, etc.)
    for (int i = 0; i < 3; i++) {
        address_info[i] = 0;
    }
    
    // Clear ID/EX pipeline registers
    for (int i = 0; i < 8; i++) {
        id_to_ex[i] = 0;
    }
    
    // Clear EX/MEM pipeline registers
    for (int i = 0; i < 5; i++) {
        ex_to_mem[i] = 0;
    }
    
    // Clear MEM/WB pipeline registers
    for (int i = 0; i < 5; i++) {
        mem_to_wb[i] = 0;
    }
}

// Run the processor
void run_processor(RegisterFile* registers, uint8_t* memory) {
    uint32_t instruction = 0;
    uint32_t address_info[3];     // Used for PC, jump/branch targets
    uint32_t id_to_ex[8];         // ID/EX pipeline registers
    uint32_t ex_to_mem[5];        // EX/MEM pipeline registers
    uint32_t mem_to_wb[5];        // MEM/WB pipeline registers
    ControlSignals control;
    
    initialize_control(&control);

    // Run until program termination (jr $ra when $ra = 0xffffffff)
    while (registers->program_counter != 0xffffffff) {
        printf("12345678> Cycle : %d\n", instruction_count);
        
        // Fetch instruction
        instruction = instruction_fetch(registers, memory);
        
        // Skip if NOP (instruction = 0)
        if (instruction == 0) {
            printf("\tNOP\n\n");
            continue;
        }
        
        // Execute pipeline stages
        instruction_decode(registers, &control, instruction, id_to_ex, address_info);
        instruction_execute(registers, &control, id_to_ex, ex_to_mem, address_info);
        memory_stage(&control, ex_to_mem, mem_to_wb);
        writeback_stage(registers, &control, mem_to_wb);
        
        printf("\n\n");
        
        // Clear pipeline registers for next cycle
        clear_pipeline_registers(&instruction, address_info, id_to_ex, ex_to_mem, mem_to_wb);
    }

    // Print statistics
    printf("12345678> Final Result");
    printf("\tCycles: %d, R-type instructions: %d, I-type instructions: %d, J-type instructions: %d\n", 
           instruction_count, rtype_count, itype_count, jtype_count);
    printf("\tReturn value(v0) : %d\n", registers->regs[2]);
}

// Initialize registers
void setup_registers(RegisterFile* registers) {
    // Initialize program counter
    registers->program_counter = 0;
    
    // Initialize all registers to 0
    for (int i = 0; i < 32; i++) {
        registers->regs[i] = 0;
    }
    
    registers->regs[31] = 0xffffffff;  // $ra (return address)
    registers->regs[29] = 0x1000000;   // $sp (stack pointer)
}

int main(int argc, char* argv[]) {
    // Check if filename is provided as command line argument
    if (argc < 2) {
        printf("Usage: %s <filename.bin>\n", argv[0]);
        printf("Available test files:\n");
        printf("  - sum.bin\n");
        printf("  - func.bin\n");
        printf("  - fibonacci.bin\n");
        printf("  - factorial.bin\n");
        printf("  - power.bin\n");
        printf("  - fib2.bin\n");
        return 1;
    }

    // Open binary file containing MIPS instructions
    FILE* file = fopen(argv[1], "rb");

    if (!file) {
        perror("File opening failed");
        return 1;
    }

    uint32_t buffer;            
    size_t bytes_read;      
    size_t memory_index = 0;   

    // Read 32-bit words from file and store in memory byte by byte
    while ((bytes_read = fread(&buffer, sizeof(uint32_t), 1, file)) > 0 && 
           memory_index < (MEMORY_SIZE - 3)) {
        // Store in memory in big-endian format (most significant byte first)
        memory[memory_index++] = (buffer >> 24) & 0xFF;
        memory[memory_index++] = (buffer >> 16) & 0xFF;  
        memory[memory_index++] = (buffer >> 8) & 0xFF;   
        memory[memory_index++] = buffer & 0xFF;         
    }

    fclose(file);

    // Initialize register file
    RegisterFile registers;
    setup_registers(&registers);
    
    // Run the processor
    run_processor(&registers, memory);

    return 0;
}