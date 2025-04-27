# 컴파일러와 옵션
CC = gcc
CFLAGS = -Wall -Wextra -O2

# 파일 이름 설정
TARGET = mips_emulator
SRC = single_cycle_main.c datapath.c
OBJS = $(SRC:.c=.o)
DEPS = datapath.h

# 기본 목표
all: $(TARGET)

# 링크 규칙
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# 소스 -> 오브젝트 컴파일 규칙
%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $<

# 정리
clean:
	rm -f $(TARGET) $(OBJS)
