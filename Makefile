# The user needs to assign these for their project
CFILES=parse_mpls.c
EXEC=parse_mpls

# The included dependency file contains all the incremental compilation info,
# however we use the implicit rule for making each one which is (simplified):
# gcc $(CFLAGS) -c -o Foo.o Foo.c
# Thus we place our compiler flags into this default variable
CFLAGS=-Wall -lm -ggdb -m32

# The main linking rule
$(EXEC): $(CFILES)
	gcc $(CFLAGS) $(CFILES) -o $(EXEC)

clean:
	rm -rf *~ *.o $(EXEC) *.dSYM
