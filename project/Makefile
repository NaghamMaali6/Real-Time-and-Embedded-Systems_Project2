# =========================================================
# Compiler
# =========================================================
CC = gcc

# =========================================================
# Compiler Flags
#
# -Wall      -> enable common warnings
# -Wextra    -> extra warnings
# -g         -> debugging support for gdb
# -Iinclude  -> look for headers in include/
# =========================================================
CFLAGS = -Wall -Wextra -g -Iinclude

# =========================================================
# Linker Flags
#
# -lGL -lGLU -lglut  -> OpenGL + GLUT
# -lm                -> math (cosf/sinf in graphics.c)
# =========================================================
LDFLAGS = -lGL -lGLU -lglut -lm

# =========================================================
# Directories
# =========================================================
SRC_DIR = src
OBJ_DIR = obj
LOG_DIR = logs

# =========================================================
# Final Executable Name
# =========================================================
BIN = traffic_system

# =========================================================
# Source and Object Files
# =========================================================
SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRC))

# =========================================================
# Default Target
# =========================================================
all: $(LOG_DIR) $(BIN)

# =========================================================
# Link Final Executable
# =========================================================
$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $(BIN) $(LDFLAGS)

# =========================================================
# Compile Each Source File
# =========================================================
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# =========================================================
# Create Directories if Missing
# =========================================================
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(LOG_DIR):
	mkdir -p $(LOG_DIR)

# =========================================================
# Run Program
# =========================================================
run: all
	./$(BIN) config/settings.txt

# =========================================================
# Run Under Valgrind (memory + IPC leak check)
# =========================================================
valgrind: all
	valgrind --leak-check=full \
	         --track-origins=yes \
	         --show-leak-kinds=all \
	         ./$(BIN) config/settings.txt

# =========================================================
# Clean Build Files Only (keep logs)
# =========================================================
clean:
	rm -rf $(OBJ_DIR) $(BIN)

# =========================================================
# Clean Logs Only
# =========================================================
clean-logs:
	rm -f $(LOG_DIR)/*.log

# =========================================================
# Clean Everything Including Logs
# =========================================================
clean-all: clean clean-logs

# =========================================================
# Rebuild Everything
# =========================================================
rebuild: clean all

# =========================================================
# Remove Stale IPC Resources
# (run this if program crashed and left orphan IPC objects)
# =========================================================
clean-ipc:
	@echo "Removing stale message queues..."
	@ipcs -q | awk 'NR>2 && NF>0 { print $$2 }' | xargs -r -I{} ipcrm -q {}
	@echo "Removing stale semaphores..."
	@ipcs -s | awk 'NR>2 && NF>0 { print $$2 }' | xargs -r -I{} ipcrm -s {}
	@echo "Removing stale shared memory..."
	@ipcs -m | awk 'NR>2 && NF>0 { print $$2 }' | xargs -r -I{} ipcrm -m {}
	@echo "IPC cleanup done."

# =========================================================
# Phony Targets
# =========================================================
.PHONY: all run valgrind clean clean-logs clean-all rebuild clean-ipc
