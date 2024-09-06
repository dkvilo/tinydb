build:
	gcc -ggdb -pedantic -Wno-discarded-qualifiers *.c -o tinydb -lpthread 