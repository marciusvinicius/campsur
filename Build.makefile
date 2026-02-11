build:
	make config=debug_arm64

run-editor:build
	./bin/Debug/camp_sur_editor

run-game:build
	./bin/Debug/camp_sur_game

run:run-editor
