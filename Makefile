DIRS := lab1 lab2 lab3 test lab4 lab5 lab6

all:
	@for dir in $(DIRS); do make -C $$dir; done

clean:
	@for dir in $(DIRS); do make -C $$dir clean; done
	rm -f out/lab* out/test

