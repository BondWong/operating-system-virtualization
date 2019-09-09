CC = gcc            # default is CC = cc
CFLAGS = -g -Wall   # default is CFLAGS = [blank]
CPPFLAGS =          # default is CPPFLAGS = [blank]
LDFLAGS = -lpthread -lvirt # default is LDFLAGS = [blank]

# default compile command: $(CC) $(CFLAGS) $(CPPFLAGS) -c -o <foo>.o <foo>.c

all: clean cpu

cpu: vcpu_scheduler.c
	$(CC) -o vcpu_scheduler $< $(CFLAGS) $(LDFLAGS)

clean:
	$(RM) -rf vcpu_scheduler
