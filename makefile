debug: src/main.c
	${CC} -g src/main.c -o bin/main -ltommath
release: src/main.c
	${CC} -O3 src/main.c -o bin/main -ltommath
clean:
	rm bin/*
format:
	find . -regex '.*\.\(cpp\|hpp\|cu\|c\|h\)' -exec clang-format -style=file -i {} \;
