# Cross-platform build (use with premake5 gmake2 for Linux, or open .sln/.slnx in VS on Windows)
# After premake5 gmake2: make -f Build.makefile build
# Windows: build in Visual Studio, then run bin\Debug\campsur_editor.exe

build:
	make config=debug_arm64

run-editor:build
	./bin/Debug/camp_sur_editor

run-game:build
	./bin/Debug/camp_sur_game
 
run: run-editor
