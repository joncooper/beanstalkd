CC=gcc
CFLAGS=-Wall -ggdb -Ilibtask -Llibtask
# TODO: only need libpthread for debugging; how do I handle this?
LDFLAGS=-lsasl2 # -lpthread # apparently statically linking with pthread causes segfaults?
# see http://forums.fedoraforum.org/archive/index.php/t-83762.html
LDLIBS=libtask/libtask.a

TARG=bauthd

# SOURCES=sasl_protocol.c connection.c bauthd.c 
# OBJECTS=$(SOURCES:.c=.o)

MOFILE=bauthd.o
OFILES=\
			 connection.o\
			 sasl_protocol.o\

HFILES=\
			ds.h

# The first target will be made if nothing is specified
# $@ is the filename of the target of the rule
# $^ is the names of all dependencies, space-separated
# $< is the name of the first dependency
# % in a target or dependency is apparently a wildcard

# See http://www.cs.swarthmore.edu/~newhall/unixhelp/howto_makefiles.html
# See also http://web.mit.edu/gnu/doc/html/make_14.html#SEC109

.PHONY: all
all: $(TARG)

#.c.o:
#	$(CC) $(LDFLAGS) $(CFLAGS) $< -c -o $@

$(OFILES) $(MOFILE): $(HFILES)

$(TARG): $(OFILES) $(MOFILE)
	$(LINK.o) -o $@ $^ $(LDLIBS)

.PHONY: clean
clean:
	rm -f *.o $(TARG)

#$(TARG): $(OFILES) $(MOFILE)
#	$(LINK.o) -o $@ $^ $(LDLIBS)

#$(OFILES) $(MOFILE): $(HFILES)
