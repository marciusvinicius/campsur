baseName = path.getbasename(os.getcwd())

project(workspaceName .. "_game")
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

includedirs({ "./" })
includedirs({ "src" })
includedirs({ "include" })

-----------------------------------------
-- LINKS
-----------------------------------------

link_to("engine")
link_raylib()
-- To link to a lib use link_to("LIB_FOLDER_NAME")
