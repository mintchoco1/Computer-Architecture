#include "structure.h"

// 간단한 2-bit 브랜치 예측기
#define BP_TABLE_SIZE 256
#define BP_STRONGLY_NOT_TAKEN 0
#define BP_WEAKLY_NOT_TAKEN   1
#define BP_WEAKLY_TAKEN       2
#define BP_STRONGLY_TAKEN     3

typedef struct {
    uint8_t predictor[BP_TABLE_SIZE];  // 2-bit predictor for each entry
    uint32_t btb[BP_TABLE_SIZE];       // Branch Target Buffer
    bool btb_valid[BP_TABLE_SIZE];     // BTB entry validity
} BranchPredictor;

static BranchPredictor bp = {0};

// PC를 인덱스로 변환
static uint32_t get_bp_index(uint32_t pc) {
    return (pc >> 2) & (BP_TABLE_SIZE - 1);
}

// 브랜치 예측 수행
BranchPrediction predict_branch(uint32_t pc) {
    BranchPrediction prediction = {0};
    uint32_t index = get_bp_index(pc);
    
    // 2-bit predictor에 따른 예측
    prediction.taken = (bp.predictor[index] >= BP_WEAKLY_TAKEN);
    
    // BTB에서 타겟 주소 가져오기
    if (bp.btb_valid[index]) {
        prediction.target = bp.btb[index];
    } else {
        prediction.target = pc + 4;  // 기본값
    }
    
    prediction.confidence = (bp.predictor[index] == BP_STRONGLY_TAKEN || 
                           bp.predictor[index] == BP_STRONGLY_NOT_TAKEN) ? 2 : 1;
    
    return prediction;
}

// 브랜치 예측 결과 업데이트
void update_branch_predictor(uint32_t pc, bool actual_taken, uint32_t actual_target) {
    uint32_t index = get_bp_index(pc);
    
    // 2-bit predictor 업데이트
    if (actual_taken) {
        if (bp.predictor[index] < BP_STRONGLY_TAKEN) {
            bp.predictor[index]++;
        }
    } else {
        if (bp.predictor[index] > BP_STRONGLY_NOT_TAKEN) {
            bp.predictor[index]--;
        }
    }
    
    // BTB 업데이트 (taken인 경우에만)
    if (actual_taken) {
        bp.btb[index] = actual_target;
        bp.btb_valid[index] = true;
    }
}

// 브랜치 예측기 초기화
void init_branch_predictor(void) {
    for (int i = 0; i < BP_TABLE_SIZE; i++) {
        bp.predictor[i] = BP_WEAKLY_NOT_TAKEN;  // 초기값: weakly not taken
        bp.btb[i] = 0;
        bp.btb_valid[i] = false;
    }
}

// 예측 상태를 문자열로 변환
const char* get_prediction_state_string(uint8_t state) {
    switch (state) {
        case BP_STRONGLY_NOT_TAKEN: return "강하게 Not-Taken";
        case BP_WEAKLY_NOT_TAKEN:   return "약하게 Not-Taken";
        case BP_WEAKLY_TAKEN:       return "약하게 Taken";
        case BP_STRONGLY_TAKEN:     return "강하게 Taken";
        default: return "Unknown";
    }
}