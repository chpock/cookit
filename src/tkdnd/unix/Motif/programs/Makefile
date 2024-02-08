
# change for your platform
X11_INCLUDES= /X11/include
EXTRA_LIBS=

CFLAGS= -I../lib -I$(X11_INCLUDES)

LIBS= -lXaw -lXmu -lXt -lX11 $(EXTRA_LIBS)


DND_OBJS= dnd.o ../lib/libDnd.a

all: dnd

dnd.o:
	gcc -I../lib -I/X11/include -c dnd.c -o dnd.o

dnd: $(DND_OBJS)
	rm -f $@
	gcc -o $@ $(DND_OBJS) -L/usr/X11R6/lib $(LIBS)

clean: 
	  rm -rf *.o dnd .*~
