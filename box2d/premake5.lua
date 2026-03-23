-- Box2D v3 static library (sources from check_box2d in root premake5.lua)

project("box2d")
kind("StaticLib")
language("C")
cdialect("C17")
location("./")
targetdir("../bin/%{cfg.buildcfg}")

vpaths({
	["Source/*"] = { "src/**.c" },
	["API/*"] = { "include/**.h" },
})
files({ "src/**.c" })

includedirs({ "./include" })
includedirs({ "./src" })

filter({ "system:linux or system:macosx" })
	buildoptions({ "-ffp-contract=off" })

filter("system:linux")
	defines({ "_POSIX_C_SOURCE=200809L" })

filter("system:windows")
	buildoptions({ "/wd4820", "/wd5045" })

filter({})
