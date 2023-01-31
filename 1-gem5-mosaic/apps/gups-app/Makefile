
all: libgupsmp.a libgups.a gups

libgupsmp.a: gupsmp.o
	ar rcs $@ $^

libgups.a: gups.o
	ar rcs $@ $^	

libgupstoy.a: gupstoy.o
	ar rcs $@ $^	

gupsmp.o : gups.c
	gcc -c -O3 -fopenmp -o $@ $< 

gups : gups.c
	gcc gups.c -o gups

gupstoy.o : gupstoy.c
	gcc -c -O3 -o $@ $< 

clean:
	rm -f *.o *.a

