CC = gcc
CFLAGS = -Wall -g
TARGET = mips_pipeline
SOURCES = main.c stage_IF.c stage_ID.c stage_EX.c stage_MEM.c stage_WB.c control.c hazzard.c

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)

clean:
	rm -f $(TARGET)

debug: CFLAGS += -DDEBUG_IF -DDEBUG_PIPELINE
debug: clean $(TARGET)

test: $(TARGET)
	./$(TARGET) test.bin

.PHONY: all clean debug test