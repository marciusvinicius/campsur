baseName = path.getbasename(os.getcwd())

project(workspaceName .. "_fp_plane")
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
	["Header Files/*"] = { "**.h", "**.hpp" },
	["Source Files/*"] = { "**.cpp", "**.c" },
})
files({ "**.cpp", "**.h", "**.hpp" })

includedirs({ "./", "../enet-1.3.18/include" })

link_to("engine")
links({ "enet" })
link_sdl3()

filter("system:linux")
	links({ "GL", "pthread", "m", "dl", "X11" })

filter("system:windows")
	systemversion("latest")

filter({})
