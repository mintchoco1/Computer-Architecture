#include "structure.h"

// 브랜치 예측 테이블 크기 (2^8 = 256 entries)
#define BRANCH_PREDICTOR_SIZE 256
#define BRANCH_PREDICTOR_MASK (BRANCH_PREDICTOR_SIZE - 1)

// 2-bit saturating counter states
typedef enum {
    STRONGLY_NOT_TAKEN = 0,   // 00
    WEAKLY_NOT_TAKEN = 1,     // 01
    WEAKLY_TAKEN = 2,         // 10
    STRONGLY_TAKEN = 3        // 11
} BranchState;

// 브랜치 예측 테이블
static BranchState branch_predictor_table[BRANCH_PREDICTOR_SIZE];
static bool predictor_initialized = false;

extern uint64_t branch_predictions;
extern uint64_t branch_correct_predictions;
extern uint64_t branch_mispredictions;

// 브랜치 예측기 초기화
void init_branch_predictor(void) {
    if (!predictor_initialized) {
        // 모든 엔트리를 WEAKLY_NOT_TAKEN으로 초기화
        for (int i = 0; i < BRANCH_PREDICTOR_SIZE; i++) {
            branch_predictor_table[i] = WEAKLY_NOT_TAKEN;
        }
        predictor_initialized = true;
        
        branch_predictions = 0;
        branch_correct_predictions = 0;
        branch_mispredictions = 0;
    }
}

static uint32_t get_predictor_index(uint32_t pc) {
    // PC의 하위 비트들을 사용 (word aligned이므로 2비트 shift)
    return (pc >> 2) & BRANCH_PREDICTOR_MASK;
}

bool predict_branch(uint32_t pc) {
    init_branch_predictor();
    
    uint32_t index = get_predictor_index(pc);
    BranchState state = branch_predictor_table[index];
    
    branch_predictions++;
    
    // WEAKLY_TAKEN 이상이면 taken으로 예측
    return (state >= WEAKLY_TAKEN);
}

void update_branch_predictor(uint32_t pc, bool actual_taken, bool predicted_taken) {
    uint32_t index = get_predictor_index(pc);
    BranchState current_state = branch_predictor_table[index];
    
    // 예측 정확도 업데이트
    if (actual_taken == predicted_taken) {
        branch_correct_predictions++;
    } else {
        branch_mispredictions++;
    }
    
    // 2-bit saturating counter 업데이트
    if (actual_taken) {
        // 브랜치가 taken됨 - counter 증가 (최대 3)
        if (current_state < STRONGLY_TAKEN) {
            branch_predictor_table[index] = current_state + 1;
        }
    } else {
        // 브랜치가 not taken됨 - counter 감소 (최소 0)
        if (current_state > STRONGLY_NOT_TAKEN) {
            branch_predictor_table[index] = current_state - 1;
        }
    }
}

void print_branch_prediction_stats(void) {
    if (branch_predictions > 0) {
        double accuracy = (double)branch_correct_predictions / branch_predictions * 100.0;
        printf("Branch Prediction Statistics:\n");
        printf("  Total predictions: %llu\n", (unsigned long long)branch_predictions);
        printf("  Correct predictions: %llu\n", (unsigned long long)branch_correct_predictions);
        printf("  Mispredictions: %llu\n", (unsigned long long)branch_mispredictions);
        printf("  Prediction accuracy: %.2f%%\n", accuracy);
    }
}

void reset_branch_predictor(void) {
    init_branch_predictor();
    branch_predictions = 0;
    branch_correct_predictions = 0;
    branch_mispredictions = 0;
}