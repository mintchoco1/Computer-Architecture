# ------------------------------------------------------------
#  MIPS Pipeline Simulator – Makefile
# ------------------------------------------------------------

# 1) Toolchain & Flags --------------------------------------------------------
CC      ?= gcc
CSTD     = c11
WARNINGS = -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion
OPT      = -O2
DEBUGOPT = -g -DDEBUG_WB

CFLAGS  ?= -std=$(CSTD) $(WARNINGS) $(OPT)
LDFLAGS ?=

# Automatically generate dependency (.d) files
CFLAGS  += -MMD -MP

# 2) Source / Object Lists ----------------------------------------------------
SRC := \
    main.c      \
    stage_IF.c  \
    stage_ID.c  \
    stage_EX.c  \
    stage_MEM.c \
    stage_WB.c  \
    control.c

OBJ := $(SRC:.c=.o)
DEP := $(OBJ:.o=.d)

TARGET := mips_sim

# 3) Default target -----------------------------------------------------------
.PHONY: all
all: $(TARGET)

# 4) Release build ------------------------------------------------------------
$(TARGET): $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) -o $@

# 5) Debug build --------------------------------------------------------------
.PHONY: debug
DEBUG_CFLAGS := $(CFLAGS:$(OPT)=$(DEBUGOPT))

debug: CFLAGS := $(DEBUG_CFLAGS)
debug: clean $(TARGET)

# 6) Clean --------------------------------------------------------------------
.PHONY: clean
clean:
	rm -f $(OBJ) $(DEP) $(TARGET)

# 7) Convenience target to run -------------------------------------------------
#    $ make run BIN=foo.bin [ENTRY=0x4000]
.PHONY: run
run: $(TARGET)
	./$(TARGET) $(BIN) $(ENTRY)

# 8) Include auto‑generated dependency files ----------------------------------
-include $(DEP)

# 9) Print configuration ------------------------------------------------------
.PHONY: info
info:
	@echo "CC      = $(CC)"
	@echo "CFLAGS  = $(CFLAGS)"
	@echo "LDFLAGS = $(LDFLAGS)"
	@echo "SRC     = $(SRC)"
