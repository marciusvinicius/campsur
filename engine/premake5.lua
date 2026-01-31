baseName = path.getbasename(os.getcwd())

project(baseName)
kind("StaticLib")
location("./")
targetdir("../bin/%{cfg.buildcfg}")

cdialect("C23")
cppdialect("C++23")

vpaths({
	["Header Files/*"] = { "include/**.h", "include/**.hpp", "**.h", "**.hpp" },
	["Source Files/*"] = { "src/**.cpp", "src/**.c", "**.cpp", "**.c" },
})
files({ "**.hpp", "**.h", "**.cpp", "**.c" })

includedirs({ "./" })
includedirs({ "./src" })
includedirs({ "./include" })
includedirs({ "../enet-1.3.18/include" })

links({ "enet" })

include_raylib()
