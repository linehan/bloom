CC=gcc
#
#                 
# optimize   warnings
# lvl 3 \     /    
CFLAGS=-O3 -Wall            
LDFLAGS= 
#      
#    
#                                  

SOURCES=test.c bloom.c
OBJECTS=$(SOURCES:.c=.o)

EXECUTABLE=test

all: $(SOURCES)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SOURCES) -o $(EXECUTABLE) 

clean:
	rm -f $(OBJECTS) $(EXECUTABLE) gmon.out 
