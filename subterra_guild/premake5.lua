baseName = path.getbasename(os.getcwd())

-- Set by repo-root premake5.lua when included; default when run from this folder alone.
if not workspaceName then
	workspaceName = path.getbasename(path.getdirectory(path.getdirectory(_SCRIPT)))
end

project(workspaceName .. "_subterra_guild")
kind("ConsoleApp")
language("C++")
cppdialect("C++23")
staticruntime("off")

location("./")
targetdir("../bin/%{cfg.buildcfg}")

filter("action:vs*")
debugdir("$(SolutionDir)")

filter({ "action:vs*", "configurations:Release" })
kind("WindowedApp")
entrypoint("mainCRTStartup")

filter({})

vpaths({
	["Header Files/*"] = { "include/**.h", "include/**.hpp", "src/**.h", "src/**.hpp", "**.h", "**.hpp" },
	["Source Files/*"] = { "src/**.c", "src/**.cpp", "**.c", "**.cpp" },
})
files({
	"src/subterra_item_light.cpp",
	"src/subterra_loadout.cpp",
	"**.c", "**.cpp", "**.h", "**.hpp",
	"../editor/thirdpart/imgui-docking/imgui.cpp",
	"../editor/thirdpart/imgui-docking/imgui_draw.cpp",
	"../editor/thirdpart/imgui-docking/imgui_tables.cpp",
	"../editor/thirdpart/imgui-docking/imgui_widgets.cpp",
	"../editor/thirdpart/imgui-docking/backends/imgui_impl_sdl3.cpp",
	"../editor/thirdpart/imgui-docking/backends/imgui_impl_sdlrenderer3.cpp",
})

filter({})

includedirs({
	"./",
	"src",
	"include",
	"../enet-1.3.18/include",
	"../editor/thirdpart/imgui-docking",
	"../editor/thirdpart/imgui-docking/backends",
})

link_to("engine")
links("enet")
link_sdl3()

filter("system:linux")
links({ "GL", "pthread", "m", "dl", "X11", "z" })

filter("system:windows")
systemversion("latest")

filter({})
