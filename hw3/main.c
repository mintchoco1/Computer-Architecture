#include "structure.h"

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