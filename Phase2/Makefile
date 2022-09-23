ALL: default 

CC       = gcc
CLINKER  = $(CC)
OPTFLAGS = -O0 


SHELL = /bin/sh

CFLAGS  = -DREENTRANT -g -std=gnu99
CCFLAGS = $(CFLAGS)
LIBS    = -lpthread

EXECS = exemple

default: $(EXECS)

$(EXECS): %: %.o dsm.o

#%.o:%.c
#	$(CC) $(CFLAGS) -c $<

%:%.o dsm.o 
	$(CLINKER) $(OPTFLAGS) -o $@ $^ $(LIBS)
	@-mv $@ ../Phase1/bin/		

clean:
	@-/bin/rm -f *.o *~ PI* $(EXECS) *.out core  

