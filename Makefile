maga.so: maga.c
	$(CC) -std=c11 -fPIC -shared -o maga.so maga.c -g -Wall -pedantic -lcsv
