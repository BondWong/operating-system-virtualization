CC = gcc            # default is CC = cc
CFLAGS = -g -Wall   # default is CFLAGS = [blank]
CPPFLAGS =          # default is CPPFLAGS = [blank]
LDFLAGS = -lpthread -lvirt # default is LDFLAGS = [blank]

# default compile command: $(CC) $(CFLAGS) $(CPPFLAGS) -c -o <foo>.o <foo>.c

all: clean memory

memory: memory_coordinator.c
	$(CC) -o memory_coordinator $< $(CFLAGS) $(LDFLAGS)

clean:
	$(RM) -rf memory_coordinator
