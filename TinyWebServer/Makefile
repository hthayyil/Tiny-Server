TARGETS=homework5 format_string

CFLAGS=-Wall -g -O0

all: $(TARGETS)

homework5: homework5.c
	gcc $(CFLAGS) -o homework5 homework5.c -lpthread


format_string: format_string.c
	gcc $(CFLAGS) -o format_string format_string.c -lpthread

clean:
	rm -f $(TARGETS)

