SOURCES = zemaphore.c
CC = g++
EXECUTABLE = zema
RM = rm -rf

all: $(SOURCES)
	$(CC) -o $(EXECUTABLE) $(SOURCES) -lpthread

clean:
	$(RM) *.o $(EXECUTABLE)