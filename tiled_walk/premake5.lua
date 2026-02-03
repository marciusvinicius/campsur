baseName = path.getbasename(os.getcwd())

project(workspaceName .. "_tiled_walk")
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
files({ "**.c", "**.cpp", "**.h", "**.hpp" })

filter({})

includedirs({ "./", "src", "include", "../enet-1.3.18/include" })

link_to("engine")
links("enet")
link_raylib()

-----------------------------------------
-- PLATFORM (cross-platform: Windows + Linux)
-----------------------------------------
filter("system:linux")
links({ "GL", "pthread", "m", "dl", "X11" })

filter("system:windows")
systemversion("latest")

filter({})
