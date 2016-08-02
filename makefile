IDIR = .
CC=gcc
CFLAGS=-I $(IDIR)

ODIR = .
LDIR = .

_DEPS = csapp.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = tiny.o csapp.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

server: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) -lpthread

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~ 
