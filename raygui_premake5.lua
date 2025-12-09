function platform_defines()
	filter({ "options:backend=GLFW" })
	defines({ "PLATFORM_DESKTOP" })

	filter({ "options:backend=RLFW" })
	defines({ "PLATFORM_DESKTOP_RGFW" })

	filter({ "options:backend=SDL2" })
	defines({ "PLATFORM_DESKTOP_SDL" })

	filter({ "options:backend=SDL3" })
	defines({ "PLATFORM_DESKTOP_SDL" })

	filter({ "options:graphics=opengl43" })
	defines({ "GRAPHICS_API_OPENGL_43" })

	filter({ "options:graphics=opengl33" })
	defines({ "GRAPHICS_API_OPENGL_33" })

	filter({ "options:graphics=opengl21" })
	defines({ "GRAPHICS_API_OPENGL_21" })

	filter({ "options:graphics=opengl11" })
	defines({ "GRAPHICS_API_OPENGL_11" })

	filter({ "options:graphics=openges3" })
	defines({ "GRAPHICS_API_OPENGL_ES3" })

	filter({ "options:graphics=openges2" })
	defines({ "GRAPHICS_API_OPENGL_ES2" })

	filter({ "system:macosx" })
	disablewarnings({ "deprecated-declarations" })

	filter({ "system:linux" })
	defines({ "_GLFW_X11" })
	defines({ "_GNU_SOURCE" })
	-- This is necessary, otherwise compilation will fail since
	-- there is no CLOCK_MONOTOMIC. raylib claims to have a workaround
	-- to compile under c99 without -D_GNU_SOURCE, but it didn't seem
	-- to work. raylib's Makefile also adds this flag, probably why it went
	-- unnoticed for so long.
	-- It compiles under c11 without -D_GNU_SOURCE, because c11 requires
	-- to have CLOCK_MONOTOMIC
	-- See: https://github.com/raysan5/raylib/issues/2729

	filter({})
end

function get_raygui_dir()
	if os.isdir("raygui-master") then
		return "raygui-master"
	end
	if os.isdir("../raygui-master") then
		return "raygui-master"
	end
	return "raygui"
end

function link_raygui()
	links({ "raygui" })
	raygui_dir = get_raygui_dir()
	includedirs({ raygui_dir })
	includedirs({ raygui_dir .. "backends" })
	includedirs({ raygui_dir .. "misc" })
	includedirs({ raygui_dir .. "misc" .. "cpp" })
	filter("files:**.dll")
	buildaction("Copy")
	filter({})
end

function include_raygui()
	raygui_dir = get_raygui_dir()
	includedirs({ raygui_dir })
	includedirs({ raygui_dir .. "backends" })
end

project("raygui")
kind("StaticLib")
raygui_dir = get_raygui_dir()
platform_defines()

location(raygui_dir)
language("C")
targetdir("bin/%{cfg.buildcfg}")

print("Using raygui dir " .. raygui_dir)
filter("system:linux")
pic("On")
systemversion("latest")
staticruntime("On")
includedirs({ raygui_dir })
vpaths({
	["Source Files/*"] = { raygui_dir .. "src/**.c" },
	["Header Files/*"] = { raygui_dir .. "src/**.h" },
})
files({ raygui_dir .. "/src/*.h", raylib_dir .. "/src/*.c" })
filter({})
