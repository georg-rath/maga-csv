CFLAGS=-Wall -pedantic -std=c11 -g -O3 -fPIC -shared -march=native

maga-csv.so: maga-csv.c
	$(CC) $(CFLAGS) -o $@ $< -lcsv
