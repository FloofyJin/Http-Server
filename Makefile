#####
# Author: Jinsung Park
# CruzId: jpark598
# Asgn - httpserver
#####

EXEC	= httpserver

SRC1	= httpserver.c helper.c queue.c
OBJ1	= $(SRC1:%.c=%.o)

CC		= clang
CFLAGS	= -Wall -Wextra -Wpedantic -Werror -pthread -g

.PHONY: all format clean $(EXEC)

all: $(EXEC)

$(EXEC): $(OBJ1)
	 $(CC) $(CFLAGS) -o $@ $^ -lm

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -rf $(EXEC) $(OBJ1)

format:
	clang-format -i -style=file *.[ch]

