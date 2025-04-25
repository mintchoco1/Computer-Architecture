# 컴파일러 및 플래그 설정
CC = gcc
CFLAGS = -Wall -Wextra -O2

# 타겟 실행 파일 이름
TARGET = mips_emulator

# 소스 및 헤더 파일
SRC = single_cycle_main.c
DEPS = dataPath.h
OBJ = $(SRC:.c=.o)

# 기본 빌드 규칙
all: $(TARGET)

# 타겟 링크
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

# 오브젝트 파일 컴파일 시, 헤더에 의존성 추가
%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $<

# 정리
clean:
	rm -f $(TARGET) $(OBJ)
