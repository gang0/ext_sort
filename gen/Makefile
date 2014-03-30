//+----------------------------------------------------+
//| Author: Maxim Ulyanov <ulyanov.maxim@gmail.com>    |
//+----------------------------------------------------+

CC	= g++

SOURCES	= gen.cpp
OBJECTS	= $(SOURCES:.cpp=.o)

CFLAGS	= -m64 -c -Wall -std=c++11

LDFLAGS	= -m64
SYSLIBS =

EXEC	= gen

all:	$(SOURCES) $(EXEC)

$(EXEC):	$(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) $(SYSLIBS) -o $@

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf *.o sort
