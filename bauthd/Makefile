CFLAGS=-Wall -ggdb -Ilibtask -Llibtask
LDFLAGS=-lsasl2 libtask/libtask.a
SOURCES=sasl_protocol.c connection.c bauthd.c 
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=bauthd

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -c -o $@
