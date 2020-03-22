all: httpd client
LIBS = -l pthread -std=c++11 
httpd: httpd.c
	g++ -g -W -Wall $(LIBS) -o $@ $<

client: simpleclient.c
	gcc -W -Wall -o $@ $<
clean:
	rm httpd
