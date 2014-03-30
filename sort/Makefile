//+----------------------------------------------------+
//| Author: Maxim Ulyanov <ulyanov.maxim@gmail.com>    |
//+----------------------------------------------------+

CC	= g++

SOURCES	= sort.cpp
OBJECTS	= $(SOURCES:.cpp=.o)

CFLAGS	= -m64 -c -Wall -std=c++11

LDFLAGS	= -m64
SYSLIBS = -lboost_filesystem -lboost_system -lboost_thread

EXEC	= sort

all:	$(SOURCES) $(EXEC)

$(EXEC):	$(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) $(SYSLIBS) -o $@

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf *.o sort
