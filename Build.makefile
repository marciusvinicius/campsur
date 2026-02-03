# Cross-platform build (use with premake5 gmake2 for Linux, or open .sln/.slnx in VS on Windows)
# After premake5 gmake2: make -f Build.makefile build
# Windows: build in Visual Studio, then run bin\Debug\campsur_editor.exe

build:
	$(MAKE)

run-editor: build
	./bin/Debug/campsur_editor

run-game: build
	./bin/Debug/campsur_game

run: run-editor
