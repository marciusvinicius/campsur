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

-- Engine uses SDL3 only (no raylib). Paths relative to repo root.
filter({ "system:windows", "platforms:x64", "action:vs*" })
	includedirs({ "../SDL3/include/SDL3", "../SDL3/include" })
	libdirs({ "../SDL3/lib/x64" })
	links({ "SDL3" })

filter({ "system:windows", "platforms:x32", "action:vs*" })
	includedirs({ "../SDL3/include/SDL3", "../SDL3/include" })
	libdirs({ "../SDL3/lib/x32" })
	links({ "SDL3" })

filter({ "system:windows", "platforms:x64", "action:gmake*" })
	includedirs({ "../SDL3/x86_64-w64-mingw32/include/SDL3", "../SDL3/x86_64-w64-mingw32/include" })
	libdirs({ "../SDL3/x86_64-w64-mingw32/lib/", "../SDL3/x86_64-w64-mingw32/bin/" })
	links({ "SDL3" })

filter({ "system:linux or system:macosx" })
	links({ "SDL3" })

filter("action:vs*")
	defines({ "_CRT_SECURE_NO_WARNINGS" })

filter({})
