program = BTree
source = btree.c
obj = $(source:.c=.o)
objomp = $(source:.c=.o-omp)

# Standard Flags
CFLAGS := -std=gnu99 -Wall  -O3

# compiler
CC = gcc


$(program): $(obj) Makefile
	$(CC) $(CFLAGS) $(obj) -o $@ $(LDFLAGS)

libbtree.a: $(obj) Makefile
	ar rcs $@ $(obj)

libbtreeomp.a: $(objomp) Makefile
	ar rcs $@ $(objomp)

%.o: %.c Makefile
	$(CC) $(CFLAGS) -c $< -o $@

%.o-omp: %.c Makefile
	$(CC) $(CFLAGS) -fopenmp -c $< -o $@

clean:
	rm -rf $(program) $(obj)
	rm -f *.a
	rm -f *.o-omp
