#include "structure.h"

#define CACHE_SET_SIZE 2048     // 캐시 세트의 개수
#define CACHE_ASSOC 4          // 4-way associative
#define CACHE_LINE_SIZE 4      // 캐시 라인 데이터 크기 

typedef struct {
    uint32_t tag;
    uint8_t data[CACHE_LINE_SIZE];
    int valid;
    int dirty;
    int lru;
    int accessed;   
} CacheLine;

typedef struct {
    CacheLine lines[CACHE_ASSOC];
} CacheSet;

typedef struct {
    CacheSet sets[CACHE_SET_SIZE];
} Cache;

Cache instruction_cache;
Cache data_cache;

double inst_cache_hit = 0;
double inst_cache_access = 0;
int inst_cold_miss = 0;
int inst_conflict_miss = 0;

double data_cache_hit = 0;
double data_cache_access = 0;
int data_cold_miss = 0;
int data_conflict_miss = 0;

int time_stamp = 0;

void init_cache() {
    for (int i = 0; i < CACHE_SET_SIZE; i++) {
        for (int j = 0; j < CACHE_ASSOC; j++) {
            instruction_cache.sets[i].lines[j].tag = 0;
            instruction_cache.sets[i].lines[j].valid = 0;
            instruction_cache.sets[i].lines[j].dirty = 0;
            instruction_cache.sets[i].lines[j].lru = 0;
            instruction_cache.sets[i].lines[j].accessed = 0;
        }
    }
    
    // 데이터 캐시 초기화
    for (int i = 0; i < CACHE_SET_SIZE; i++) {
        for (int j = 0; j < CACHE_ASSOC; j++) {
            data_cache.sets[i].lines[j].tag = 0;
            data_cache.sets[i].lines[j].valid = 0;
            data_cache.sets[i].lines[j].dirty = 0;
            data_cache.sets[i].lines[j].lru = 0;
            data_cache.sets[i].lines[j].accessed = 0;
        }
    }
    
    // 통계 초기화
    inst_cache_hit = 0;
    inst_cache_access = 0;
    inst_cold_miss = 0;
    inst_conflict_miss = 0;
    
    data_cache_hit = 0;
    data_cache_access = 0;
    data_cold_miss = 0;
    data_conflict_miss = 0;
    
    time_stamp = 0;
    
    printf("Cache initialized: %d sets, %d-way associative, %d bytes per line\n", 
           CACHE_SET_SIZE, CACHE_ASSOC, CACHE_LINE_SIZE);
}

// LRU 업데이트 함수
void update_lru(CacheSet* set, int accessed_index) {
    int old_lru = set->lines[accessed_index].lru;

    for (int i = 0; i < CACHE_ASSOC; i++) {
        if (set->lines[i].lru < old_lru) {
            set->lines[i].lru++;
        }
    }
    set->lines[accessed_index].lru = 0;
}

// LRU victim 찾기 함수 
int find_lru_index(CacheSet* set) {
    int max_lru = -1;
    int lru_index = 0;

    for (int i = 0; i < CACHE_ASSOC; i++) {
        if (set->lines[i].lru > max_lru) {
            max_lru = set->lines[i].lru;
            lru_index = i;
        }
    }
    return lru_index;
}

// 캐시 히트 체크 함수 
int cache_check_hit(Cache* cache, uint32_t address, uint32_t* set_index, uint32_t* tag, double* hit_count, double* access_count) {
    (*access_count)++;
    *tag = address / (CACHE_SET_SIZE * CACHE_LINE_SIZE);
    *set_index = (address / CACHE_LINE_SIZE) % CACHE_SET_SIZE;

    CacheSet* set = &cache->sets[*set_index];
    int found_invalid_line = 0;

    for (int i = 0; i < CACHE_ASSOC; i++) {
        if (set->lines[i].valid && set->lines[i].tag == *tag) {
            (*hit_count)++;
            return i; // hit, 반환값은 히트된 라인의 인덱스
        }
        if (!set->lines[i].valid) {
            found_invalid_line = 1;
        }
    }

    // miss인 경우 - cold miss 체크
    if (found_invalid_line) {
        for (int i = 0; i < CACHE_ASSOC; i++) {
            if (!set->lines[i].accessed) {
                set->lines[i].accessed = 1;
                return -1; // cold miss
            }
        }
    }

    return -1; // conflict miss
}

// 캐시 미스 처리 함수 
int cache_handle_miss(Cache* cache, uint32_t address, uint32_t set_index, uint32_t tag, int* cold_miss, int* conflict_miss) {
    uint32_t block_offset = address % CACHE_LINE_SIZE;
    CacheSet* set = &cache->sets[set_index];

    // cold miss인지 확인
    bool is_cold_miss = false;
    for (int i = 0; i < CACHE_ASSOC; i++) {
        if (!set->lines[i].valid) {
            is_cold_miss = true;
            break;
        }
    }

    if (is_cold_miss) {
        (*cold_miss)++;
    } else {
        (*conflict_miss)++;
    }

    // LRU 알고리즘을 사용하여 교체할 라인을 선택
    int lru_index = find_lru_index(set);

    // 선택된 캐시 라인이 더티 상태인 경우, 데이터를 메모리에 씀
    if (set->lines[lru_index].dirty) {
        uint32_t write_back_address = (set->lines[lru_index].tag * (CACHE_SET_SIZE * CACHE_LINE_SIZE)) + (set_index * CACHE_LINE_SIZE);
        for (int i = 0; i < CACHE_LINE_SIZE; i++) {
            memory[write_back_address + i] = set->lines[lru_index].data[i];
        }
    }

    // 메모리에서 데이터를 읽어와 캐시에 저장
    uint32_t new_address = address - block_offset;
    for (int i = 0; i < CACHE_LINE_SIZE; i++) {
        set->lines[lru_index].data[i] = memory[new_address + i];
    }
    set->lines[lru_index].tag = tag;
    set->lines[lru_index].valid = 1;
    set->lines[lru_index].dirty = 0;

    // LRU 업데이트
    update_lru(set, lru_index);
    return lru_index;
}

// 명령어 페치 함수
uint32_t cache_read_instruction(uint32_t address) {
    uint32_t temp = 0;
    uint32_t set_index, tag;

    // 캐시 히트 여부 확인
    int hit_index = cache_check_hit(&instruction_cache, address, &set_index, &tag, &inst_cache_hit, &inst_cache_access);

    if (hit_index != -1) { // 캐시 히트
        CacheSet* set = &instruction_cache.sets[set_index];
        CacheLine* line = &set->lines[hit_index];

        // 캐시에서 명령어를 읽어옴 (리틀엔디안)
        for (int i = 0; i < 4; i++) {
            temp = temp << 8;
            temp |= line->data[3 - i];
        }

        update_lru(set, hit_index);
        printf("[I-CACHE] Hit: PC=0x%08x, Set=%d, Tag=0x%x, Line=%d\n", 
               address, set_index, tag, hit_index);
    }
    else { // 캐시 미스
        int lru_index = cache_handle_miss(&instruction_cache, address, set_index, tag, &inst_cold_miss, &inst_conflict_miss);
        CacheSet* set = &instruction_cache.sets[set_index];
        CacheLine* line = &set->lines[lru_index];

        // 캐시에서 명령어 읽기
        for (int i = 0; i < 4; i++) {
            temp = temp << 8;
            temp |= line->data[3 - i];
        }

        printf("[I-CACHE] Miss: PC=0x%08x, Set=%d, Tag=0x%x, Line=%d\n", 
               address, set_index, tag, lru_index);
    }

    return temp;
}

// 데이터 메모리 읽기 함수 
uint32_t cache_read_data(uint32_t address) {
    uint32_t set_index, tag;
    int hit_index = cache_check_hit(&data_cache, address, &set_index, &tag, &data_cache_hit, &data_cache_access);

    if (hit_index != -1) { // 캐시 히트
        CacheSet* set = &data_cache.sets[set_index];
        CacheLine* line = &set->lines[hit_index];

        // 캐시에서 데이터를 읽어옴
        uint32_t data = 0;
        for (int i = 0; i < 4; i++) {
            data |= line->data[i] << (8 * (3 - i));
        }

        // LRU 업데이트
        update_lru(set, hit_index);
        printf("[D-CACHE] Read Hit: Addr=0x%08x, Set=%d, Tag=0x%x, Line=%d\n", 
               address, set_index, tag, hit_index);
        
        return data;
    }
    else { // 캐시 미스
        int lru_index = cache_handle_miss(&data_cache, address, set_index, tag, &data_cold_miss, &data_conflict_miss);
        CacheSet* set = &data_cache.sets[set_index];
        CacheLine* line = &set->lines[lru_index];

        uint32_t data = 0;
        for (int i = 0; i < 4; i++) {
            data |= memory[address + i] << (8 * (3 - i));
            line->data[i] = memory[address + i];
        }
        
        printf("[D-CACHE] Read Miss: Addr=0x%08x, Set=%d, Tag=0x%x, Line=%d\n", 
               address, set_index, tag, lru_index);
               
        return data;
    }
}

// 데이터 메모리 쓰기 함수 
void cache_write_data(uint32_t address, uint32_t data) {
    uint32_t set_index, tag;
    int hit_index = cache_check_hit(&data_cache, address, &set_index, &tag, &data_cache_hit, &data_cache_access);

    if (hit_index != -1) { // 캐시 히트
        CacheSet* set = &data_cache.sets[set_index];
        CacheLine* line = &set->lines[hit_index];

        // 데이터 저장
        for (int i = 0; i < 4; i++) {
            line->data[i] = (data >> (8 * (3 - i))) & 0xFF;
        }

        // 해당 캐시 라인을 더티 상태로 표시
        line->dirty = 1;

        // LRU 업데이트
        update_lru(set, hit_index);
        printf("[D-CACHE] Write Hit: Addr=0x%08x, Set=%d, Tag=0x%x, Line=%d\n", 
               address, set_index, tag, hit_index);
    }
    else { // 캐시 미스
        int lru_index = cache_handle_miss(&data_cache, address, set_index, tag, &data_cold_miss, &data_conflict_miss);
        CacheSet* set = &data_cache.sets[set_index];
        CacheLine* line = &set->lines[lru_index];

        // 새 데이터 저장
        for (int i = 0; i < 4; i++) {
            line->data[i] = (data >> (8 * (3 - i))) & 0xFF;
        }
        line->dirty = 1;

        printf("[D-CACHE] Write Miss: Addr=0x%08x, Set=%d, Tag=0x%x, Line=%d\n", 
               address, set_index, tag, lru_index);
    }
}

// 캐시 플러시 함수
void cache_flush(void) {
    // 데이터 캐시의 모든 더티 라인을 메모리에 write-back
    for (int i = 0; i < CACHE_SET_SIZE; i++) {
        for (int j = 0; j < CACHE_ASSOC; j++) {
            CacheLine* line = &data_cache.sets[i].lines[j];
            if (line->dirty && line->valid) {
                uint32_t line_address = (line->tag * CACHE_SET_SIZE + i) * CACHE_LINE_SIZE;
                for (int k = 0; k < CACHE_LINE_SIZE; k++) {
                    if (line_address + k < MEMORY_SIZE) {
                        memory[line_address + k] = line->data[k];
                    }
                }
            }
        }
    }
    printf("[CACHE] Flushed all dirty lines to memory\n");
}

void print_cache_statistics(void) {
    printf("================================================================================\n");
    printf("Cache Statistics:\n");
    printf("================================================================================\n");
    
    printf("Instruction Cache:\n");
    printf("  cache access                         : %.0f\n", inst_cache_access);
    printf("  hit count                            : %.0f\n", inst_cache_hit);
    printf("  cold miss                            : %d\n", inst_cold_miss);
    printf("  conflict miss                        : %d\n", inst_conflict_miss);
    if (inst_cache_access > 0) {
        printf("  hit rate                             : %.3f %%\n", 100 * (inst_cache_hit / inst_cache_access));
    }
    
    printf("\nData Cache:\n");
    printf("  cache access                         : %.0f\n", data_cache_access);
    printf("  hit count                            : %.0f\n", data_cache_hit);
    printf("  cold miss                            : %d\n", data_cold_miss);
    printf("  conflict miss                        : %d\n", data_conflict_miss);
    if (data_cache_access > 0) {
        printf("  hit rate                             : %.3f %%\n", 100 * (data_cache_hit / data_cache_access));
    }
    
    double total_access = inst_cache_access + data_cache_access;
    double total_hit = inst_cache_hit + data_cache_hit;
    
    printf("\nOverall Cache Performance:\n");
    printf("  total cache access                   : %.0f\n", total_access);
    printf("  total hit count                      : %.0f\n", total_hit);
    if (total_access > 0) {
        printf("  overall hit rate                     : %.3f %%\n", 100 * (total_hit / total_access));
        
    }
}

void print_cache_configuration(void) {
    printf("Cache Configuration:\n");
    printf("  Cache sets: %d\n", CACHE_SET_SIZE);
    printf("  Associativity: %d-way\n", CACHE_ASSOC);
    printf("  Cache line size: %d bytes\n", CACHE_LINE_SIZE);
    printf("  Total cache size: %d bytes\n", CACHE_SET_SIZE * CACHE_ASSOC * CACHE_LINE_SIZE);
    printf("  Replacement policy: LRU\n");
    printf("  Write policy: Write-back, Write-allocate\n");
    printf("  Cache access latency: 1 cycle\n");
    printf("  Memory access latency: 1000 cycles\n");
}