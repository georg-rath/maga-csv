CFLAGS=-Wall -pedantic -std=c11 -g -O3 -fPIC -shared -march=native

maga.so: maga.c
	$(CC) $(CFLAGS) -o $@ $< -lcsv
