CC = g++
CFLAGS = -g -Wall

all: proxy

proxy: ProxyServerWithCache.c
	$(CC) $(CFLAGS) -o ProxyParser.o -c ProxyParser.c -lpthread
	$(CC) $(CFLAGS) -o Proxy.o -c ProxyServerWithCache.c -lpthread
	$(CC) $(CFLAGS) -o Proxy ProxyParser.o Proxy.o -lpthread

clean:
	rm -f proxy *.o
tar:
	tar -cvzf ass1.tgz ProxyServerWithCache.c README Makefile ProxyParser.c ProxyParser.h
