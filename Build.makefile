building:
	premake5 gmake2
run:
	make && ./bin/Debug/campsur
all:building run
	
