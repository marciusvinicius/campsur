baseName = path.getbasename(os.getcwd())

project(workspaceName .. "snakegame")
kind("ConsoleApp")
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
	["Application Resource Files/*"] = { "src/**.rc", "src/**.ico" },
})
files({ "**.c", "**.cpp", "**.h", "**.hpp" })

filter("system:windows")
files({ "src/**.rc", "src/**.ico" })
resincludedirs({ "src/**" })
filter({})

filter("files:**.ico")
buildaction("Embed")

filter({})

includedirs({ "./", "src", "include", "../enet-1.3.18/include" })

link_to("engine")
links("enet")
link_sdl3()

-----------------------------------------
-- PLATFORM (cross-platform: Windows + Linux)
-----------------------------------------
filter("system:linux")
links({ "GL", "pthread", "m", "dl", "X11" })

filter("system:windows")
systemversion("latest")

filter({})
-- To link to a lib use link_to("LIB_FOLDER_NAME")
