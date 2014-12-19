all:
	gcc -lrt -pthread -ansi -Wall -pedantic -D_SVID_SOURCE=1 -D _BSD_SOURCE -o uoc_desk uoc_desk.c
	gcc -lrt -pthread -ansi -Wall -pedantic -D_SVID_SOURCE=1 -D _BSD_SOURCE -o csd_students csd_students.c
	gcc -lrt -pthread -ansi -Wall -pedantic -D_SVID_SOURCE=1 -D _BSD_SOURCE -o math_students math_students.c
clean:
	rm -rf *.o *.c~ *.txt~ uoc_desk csd_students math_students makefile~
