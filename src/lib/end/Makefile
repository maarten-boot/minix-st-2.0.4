# Makefile for lib/end.

CC1	= $(CC) -c

LIBRARY	= ../end.a
all:	$(LIBRARY)


OBJECTS	= \
	$(LIBRARY)(edata.o) \
	$(LIBRARY)(em_end.o) \
	$(LIBRARY)(end.o) \
	$(LIBRARY)(etext.o) \

$(LIBRARY):	$(OBJECTS)
	aal cr $@ *.o
	rm *.o

$(LIBRARY)(edata.o):	edata.s
	$(AS) edata.s
	# $(CC1) edata.s

$(LIBRARY)(em_end.o):	em_end.s
	$(AS) em_end.s

$(LIBRARY)(end.o):	end.s
	$(AS) end.s

$(LIBRARY)(etext.o):	etext.s
	$(AS) etext.s
