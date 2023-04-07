workspace "ClusteredDnsServer"
    language        "C++"
    kind            "StaticLib"
    cppdialect      "c++17"
    systemversion   "10.0.19041.0" -- 10.0.18362.0
    targetdir       "build/%{cfg.action}/bin/%{prj.name}/%{cfg.platform}/%{cfg.buildcfg}"
    objdir          "!build/%{cfg.action}/obj/%{prj.name}/%{cfg.platform}/%{cfg.buildcfg}"
    startproject    "Server"
    debugdir        "run"

    platforms {
        "Win64",
    }

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
            "_DEBUG"
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

    defines {
        "_WINSOCK_DEPRECATED_NO_WARNINGS",
        "_CRT_SECURE_NO_WARNINGS",
        "NL_ARCHITECTURE_X64"
    }

    include "projects/server/server.lua"
