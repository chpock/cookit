
# change for your platform
X11_INCLUDES= /X11/include

CFLAGS= -I. -I$(X11_INCLUDES)

DND_OBJS= Dnd.o Target.o

libDnd.a: $(DND_OBJS)
	rm -f $@
	ar cq $@ $(DND_OBJS)

clean: 
	  rm -rf *.o libDnd.a .*~
