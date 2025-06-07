CC = gcc
CFLAGS = -Wall -g -std=c99
TARGET = mips_pipeline
SOURCES = main.c stage_IF.c stage_ID.c stage_EX.c stage_MEM.c stage_WB.c control.c hazzard.c branch_pre.c

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)

clean:
	rm -f $(TARGET)

# 디버그 빌드 - 모든 디버그 플래그 활성화
debug: CFLAGS += -DDEBUG_IF -DDEBUG_PIPELINE -DDEBUG_MEM -DDEBUG_HAZARD
debug: clean $(TARGET)

# 브랜치 예측 활성화 빌드
branch_pred: CFLAGS += -DENABLE_BRANCH_PREDICTION
branch_pred: clean $(TARGET)

# 디버그 + 브랜치 예측
debug_all: CFLAGS += -DDEBUG_IF -DDEBUG_PIPELINE -DDEBUG_MEM -DDEBUG_HAZARD -DENABLE_BRANCH_PREDICTION
debug_all: clean $(TARGET)

# 최적화 빌드
release: CFLAGS += -O2 -DNDEBUG
release: clean $(TARGET)

test: $(TARGET)
	./$(TARGET) test.bin

test_debug: debug
	./$(TARGET) test.bin

help:
	@echo "Available targets:"
	@echo "  all         - Build normal version"
	@echo "  debug       - Build with all debug output"
	@echo "  branch_pred - Build with branch prediction enabled"
	@echo "  debug_all   - Build with debug + branch prediction"
	@echo "  release     - Build optimized version"
	@echo "  test        - Build and run with test.bin"
	@echo "  clean       - Remove built files"

.PHONY: all clean debug branch_pred debug_all release test test_debug help