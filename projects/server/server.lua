project "Server"
    kind        "ConsoleApp"
    targetname  "clusterdns"
    --dependson   "Engine"

    filter "system:windows" -- Makefile otherwise attempts to compile StdAfx.h ...
        pchheader   "StdAfx.h"
    
    filter {}
        pchsource   "src/StdAfx.cpp"

    --links { "Engine" }

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
        "include"
    }

    lib.addStandardLibs()
    lib.addNativeLib()
