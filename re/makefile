CC = gcc
CFLAGS = -Wall -Wextra -g
TARGET = single_cycle
OBJS = main2.o datapath2.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

main2.o: main2.c datapath2.h
	$(CC) $(CFLAGS) -c main2.c

datapath2.o: datapath2.c datapath2.h
	$(CC) $(CFLAGS) -c datapath2.c

clean:
	rm -f $(OBJS) $(TARGET)
