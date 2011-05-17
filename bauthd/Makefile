CFLAGS=-Wall -ggdb 
LDFLAGS=-lpthread -lsasl2 -Ilibtask -Llibtask
SOURCES=sasl_protocol.c connection.c bauthd.c 
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=bauthd

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) libtask/libtask.a -o $@

.c.o:
	$(CC) $(LDFLAGS) $(CFLAGS) $< -c -o $@
