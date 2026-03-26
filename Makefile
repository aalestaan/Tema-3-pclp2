CC=gcc
CFLAGS=-Wall -Wextra -std=c99

# Source files.
SRCS=$(wildcard *.c)
OBJS=$(SRCS:%.c=%.o)
TARGETS=$(OBJS:%.o=%)

# Auto-generated targets.`
build: $(OBJS) $(TARGETS)

# Pack the solution into a zip file.
pack:
	zip -FSr 3XYCA_FirstnameLastname_Tema0.zip README Makefile *.c *.h

# Clean the solution.
clean:
	rm -f $(TARGETS) $(OBJS)

.PHONY: pack clean
