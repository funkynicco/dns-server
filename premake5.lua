workspace "dns-server"
    language        "C++"
    kind            "ConsoleApp"
    targetdir       "build/%{cfg.action}/bin/%{cfg.longname}"
    objdir          "build/%{cfg.action}/obj/%{prj.name}/%{cfg.longname}"
    --characterset    "MBCS"
    cppdialect      "c++17"
    systemversion   "10.0.18362.0"

    defines {
        "_LIB"
    }

    platforms {
        "x64",
    }

    filter "platforms:x64"
        architecture "x64"
        defines {
            "NL_ARCHITECTURE_X64"
		}
    
    filter {}

    configurations {
        "Debug",
        "Release",
    }
    
    flags {
        "MultiProcessorCompile"
    }

    filter "configurations:Debug"
        symbols "on"

        defines {
            "_DEBUG",
            "_CRT_SECURE_NO_WARNINGS"
        }
    
    filter "configurations:Release"
        optimize        "on"
        staticruntime   "on"

        defines {
            "NDEBUG"
        }
    
        flags {
            "NoIncrementalLink",
            "LinkTimeOptimization",
        }
    
    filter {}

    project "dns-server"

        pchheader "StdAfx.h"
        pchsource "src/StdAfx.cpp"

        files {
            "src/**.cpp",
            "src/**.h",
            "src/**.inl",
            "*.py",
            "*.natvis",
            "*.lua"
        }

        includedirs {
            "src",
            "include",
        }

        postbuildcommands {
            "py publish.py %{cfg.platform} %{cfg.buildcfg} %{cfg.action}"
        }