CC = gcc
CFLAGS = -g -std=c99 -Wall
SOURCES = main.c stage_IF.c stage_ID.c stage_EX.c stage_MEM.c stage_WB.c control.c hazard.c branch_pre.c cache.c
TARGET = mips_pipeline

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)

clean:
	rm -f $(TARGET) $(TARGET).exe

.PHONY: clean