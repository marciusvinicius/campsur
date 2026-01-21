build:
	make

run-editor:build
	./bin/Debug/camp_sur_cpp_editor

run-game:build
	./bin/Debug/camp_sur_cpp_game

run:run-editor
