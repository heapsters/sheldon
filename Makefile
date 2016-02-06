# Makefile for the CS:APP Shell Lab

VERSION = 1
HANDINDIR = /afs/cs/academic/class/15213-f02/L5/handin
DRIVER = ./sdriver.pl
MPSH = ./main
TSHREF = ./tshref
TSHARGS = "-p"
CC = gcc
CFLAGS = -Wall -O2
FILES = $(MPSH)
ROUTINES = ./routines/myspin ./routines/mysplit ./routines/mystop ./routines/myint

## gcc -Wall -02 main.c -o main

# all: $(FILES) $(ROUTINES)
all:
	gcc -Wall -O2 cmd.c -o cmd.o -c
	gcc -Wall -O2 handler.c -o handler.o -c
	gcc -Wall -O2 job.c -o job.o -c
	gcc -Wall -O2 util.c -o util.o -c
	gcc -Wall -O2 wrapper.c -o wrapper.o -c
	gcc -Wall -O2 main.c -o main.o -c
	gcc -o mpsh main.o cmd.o handler.o job.o util.o wrapper.o

##################
# Regression tests
##################

# Run tests using the student's shell program
test01:
	$(DRIVER) -t traces/trace01.txt -s $(MPSH) -a $(TSHARGS)
test02:
	$(DRIVER) -t traces/trace02.txt -s $(MPSH) -a $(TSHARGS)
test03:
	$(DRIVER) -t traces/trace03.txt -s $(MPSH) -a $(TSHARGS)
test04:
	$(DRIVER) -t traces/trace04.txt -s $(MPSH) -a $(TSHARGS)
test05:
	$(DRIVER) -t traces/trace05.txt -s $(MPSH) -a $(TSHARGS)
test06:
	$(DRIVER) -t traces/trace06.txt -s $(MPSH) -a $(TSHARGS)
test07:
	$(DRIVER) -t traces/trace07.txt -s $(MPSH) -a $(TSHARGS)
test08:
	$(DRIVER) -t traces/trace08.txt -s $(MSPH) -a $(TSHARGS)
test09:
	$(DRIVER) -t traces/trace09.txt -s $(MPSH) -a $(TSHARGS)
test10:
	$(DRIVER) -t traces/trace10.txt -s $(MPSH) -a $(TSHARGS)
test11:
	$(DRIVER) -t traces/trace11.txt -s $(MPSH) -a $(TSHARGS)
test12:
	$(DRIVER) -t traces/trace12.txt -s $(MPSH) -a $(TSHARGS)
test13:
	$(DRIVER) -t traces/trace13.txt -s $(MPSH) -a $(TSHARGS)
test14:
	$(DRIVER) -t traces/trace14.txt -s $(MPSH) -a $(TSHARGS)
test15:
	$(DRIVER) -t traces/trace15.txt -s $(MPSH) -a $(TSHARGS)
test16:
	$(DRIVER) -t traces/trace16.txt -s $(MPSH) -a $(TSHARGS)

# Run the tests using the reference shell program
rtest01:
	$(DRIVER) -t traces/trace01.txt -s $(TSHREF) -a $(TSHARGS)
rtest02:
	$(DRIVER) -t traces/trace02.txt -s $(TSHREF) -a $(TSHARGS)
rtest03:
	$(DRIVER) -t traces/trace03.txt -s $(TSHREF) -a $(TSHARGS)
rtest04:
	$(DRIVER) -t traces/trace04.txt -s $(TSHREF) -a $(TSHARGS)
rtest05:
	$(DRIVER) -t traces/trace05.txt -s $(TSHREF) -a $(TSHARGS)
rtest06:
	$(DRIVER) -t traces/trace06.txt -s $(TSHREF) -a $(TSHARGS)
rtest07:
	$(DRIVER) -t traces/trace07.txt -s $(TSHREF) -a $(TSHARGS)
rtest08:
	$(DRIVER) -t traces/trace08.txt -s $(TSHREF) -a $(TSHARGS)
rtest09:
	$(DRIVER) -t traces/trace09.txt -s $(TSHREF) -a $(TSHARGS)
rtest10:
	$(DRIVER) -t traces/trace10.txt -s $(TSHREF) -a $(TSHARGS)
rtest11:
	$(DRIVER) -t traces/trace11.txt -s $(TSHREF) -a $(TSHARGS)
rtest12:
	$(DRIVER) -t traces/trace12.txt -s $(TSHREF) -a $(TSHARGS)
rtest13:
	$(DRIVER) -t traces/trace13.txt -s $(TSHREF) -a $(TSHARGS)
rtest14:
	$(DRIVER) -t traces/trace14.txt -s $(TSHREF) -a $(TSHARGS)
rtest15:
	$(DRIVER) -t traces/trace15.txt -s $(TSHREF) -a $(TSHARGS)
rtest16:
	$(DRIVER) -t traces/trace16.txt -s $(TSHREF) -a $(TSHARGS)

# clean up
clean:
	rm -f $(FILES) *.o *~
	rm -f $(ROUTINES) *.o *~
