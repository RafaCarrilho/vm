.PHONY: clean
vm: main.c
	gcc -pthread -Wall -Wextra -std=c11 main.c -o vm

clean:
	rm -f vm