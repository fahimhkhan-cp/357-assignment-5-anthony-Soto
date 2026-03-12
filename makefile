CC = gcc
CFLAGS = -Wall -Wextra -g -std=c99 
TARGET = httpd
OBJS = httpd.o net.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

httpd.o: httpd.c net.h
	$(CC) $(CFLAGS) -c httpd.c

net.o: net.c net.h
	$(CC) $(CFLAGS) -c net.c

clean:
	rm -f $(TARGET) $(OBJS) cgi_*.tmp
	@echo "Cleanup complete."

.PHONY: all clean
