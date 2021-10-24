all:
	gcc -o webserver webserver.c -l pthread

clean:
	-rm webserver