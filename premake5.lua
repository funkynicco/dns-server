workspace "dns-server"
    language        "C++"
    kind            "ConsoleApp"
    targetdir       "build/%{cfg.action}/bin/%{cfg.longname}"
    objdir          "build/%{cfg.action}/obj/%{prj.name}/%{cfg.longname}"
    --characterset    "MBCS"
    cppdialect      "c++17"
    systemversion   "10.0.19041.0"
    --systemversion   "10.0.18362.0"

    defines {
        "__DNS_SERVER_TEST",
        --"__DOCKER",
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
            "_CRT_SECURE_NO_WARNINGS",
            "_WINSOCK_DEPRECATED_NO_WARNINGS"
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
            "src/Main.cpp",
            "src/StdAfx.cpp",
            "src/BootstrapperTest.cpp",
            "src/Bootstrapper/Bootstrapper.cpp",
            "src/Configuration/Configuration.cpp",
            "src/Console/Console.cpp",
            "src/DNS/Hosts/DnsHosts.cpp",
            "src/DNS/Hosts/DnsHostsResolve.cpp",
            "src/DNS/Statistics/DnsStatistics.cpp",
            "src/DNS/DnsAllocation.cpp",
            "src/DNS/DnsRequestHandler.cpp",
            "src/DNS/DnsRequestTimeoutHandler.cpp",
            "src/DNS/DnsResolve.cpp",
            "src/DNS/DnsServer.cpp",
            "src/DNS/DnsServerIO.cpp",
            "src/HitLog/HitLog.cpp",
            "src/Json/Json.cpp",
            "src/Logger/Logger.cpp",
            "src/Networking/SocketPool.cpp",
            "src/Utilities/ErrorDescriptionTable.cpp",
            "src/Utilities/Scanner.cpp",
            "src/Utilities/Utilities.cpp",
            "src/Web/WebAllocation.cpp",
            "src/Web/WebServer.cpp",
            "src/Web/WebServerIO.cpp",
            "src/Libraries/sqlite3/sqlite3.c",
            "src/**.h",
            "src/**.inl",
            "*.py",
            "*.natvis",
            "*.lua"
        }

        filter { "files:src/Libraries/sqlite3/sqlite3.c" }
            flags { "NoPCH" }
        
        filter {}

        includedirs {
            "src",
            "include",
        }

--        postbuildcommands {
--            "py publish.py %{cfg.platform} %{cfg.buildcfg} %{cfg.action}"
--        }