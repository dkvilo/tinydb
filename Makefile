build:
	clang -glldb -pedantic -Wno-strict-prototypes -Wno-newline-eof -Wno-ignored-qualifiers *.c -o tinydb -lpthread 