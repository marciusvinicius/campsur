baseName = path.getbasename(os.getcwd())

project(workspaceName)
kind("ConsoleApp")
location("./")
targetdir("../bin/%{cfg.buildcfg}")

cdialect("C23")
cppdialect("C++23")

filter("action:vs*")
debugdir("$(SolutionDir)")

filter({ "action:vs*", "configurations:Release" })
kind("WindowedApp")
entrypoint("mainCRTStartup")

filter("configurations:*")
defines({ "_GLIBCXX_ASSERTIONS" })

filter("configurations:Debug")
symbols("On")
optimize("Off")

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

link_to("engine")
link_raylib()
--link_raygui()
--link_to("../bin/Debug/ImGui")

-- To link to a lib use link_to("LIB_FOLDER_NAME")
