project("enet")
kind("SharedLib")
language("C")

files({ "*.c" })
includedirs({ "include/" })

defines({ "ENET_DLL=1" })

filter("configurations:Debug")
defines({ "DEBUG" })
symbols("On")
targetsuffix("d")

filter("configurations:Release")
defines({ "NDEBUG" })
optimize("On")

filter({ "configurations:Debug", "platforms:x64" })
targetsuffix("64d")

filter({ "configurations:Release", "platforms:x64" })
targetsuffix("64")

filter("system:linux")
defines({ "_DEFAULT_SOURCE" })

filter({})
