CC = gcc
CFLAGS = -Wall -Wextra -g
TARGET = single_cycle2
OBJS = main2.o datapath3.o control_signal.o alu_control.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

main2.o: main2.c datapath3.h
	$(CC) $(CFLAGS) -c main2.c

datapath3.o: datapath3.c datapath3.h
	$(CC) $(CFLAGS) -c datapath3.c

control_signal.o: control_signal.c datapath3.h
	$(CC) $(CFLAGS) -c control_signal.c

alu_control.o: alu_control.c datapath3.h
	$(CC) $(CFLAGS) -c alu_control.c

clean:
	rm -f $(OBJS) $(TARGET)
