-- Editor: cross-platform (Windows + Linux). Use premake5 gmake2 for Linux, VS for Windows.
project(workspaceName .. "_editor")
kind("ConsoleApp")
language("C++")
cppdialect("C++23")
staticruntime("off")

targetdir("../bin/%{cfg.buildcfg}")
objdir("../bin-int/%{prj.name}/%{cfg.buildcfg}")

-----------------------------------------
-- SOURCE FILES (ONLY WHAT YOU NEED)
-----------------------------------------
files({
	"src/**.cpp",
	"src/**.h",
	"include/**.h",

	"thirdpart/tinyfiledialogs.c",

	"thirdpart/imgui-docking/imgui.cpp",
	"thirdpart/imgui-docking/imgui_draw.cpp",
	"thirdpart/imgui-docking/imgui_tables.cpp",
	"thirdpart/imgui-docking/imgui_widgets.cpp",

	"thirdpart/imgui-docking/backends/imgui_impl_sdl3.cpp",
	"thirdpart/imgui-docking/backends/imgui_impl_sdlrenderer3.cpp",
})

-----------------------------------------
-- INCLUDE DIRECTORIES
-----------------------------------------
includedirs({
	"src",
	"include",

	"../engine/include",
	"../enet-1.3.18/include",

	-- tinyfiledialogs
	"thirdpart",

	"thirdpart/imgui-docking",
	"thirdpart/imgui-docking/backends",
})

-----------------------------------------
-- LINKS
-----------------------------------------
links({
	"engine",
	"enet",
})

-- Engine uses SDL3; editor uses SDL3 for window + ImGui backend (imgui_impl_sdl3).
link_sdl3()

-----------------------------------------
-- PLATFORM
-----------------------------------------
filter("system:linux")
links({
	"GL",
	"pthread",
	"m",
	"dl",
	"X11",
})

filter("system:windows")
systemversion("latest")

-----------------------------------------
-- CONFIGS
-----------------------------------------
filter("configurations:Debug")
symbols("On")
optimize("Off")

filter("configurations:Release")
optimize("On")

filter({})