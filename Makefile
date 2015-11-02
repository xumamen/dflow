ALL:shash

shash:shash.c
	gcc -Wall -O0 shash.c -o shash -lm


clean:
	rm -rf shash
	
