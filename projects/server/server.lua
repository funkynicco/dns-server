project "Server"
    kind        "ConsoleApp"
    targetname  "ClusteredDnsServer"
    --dependson   "Engine"
    pchheader   "StdAfx.h"
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
        "include",
        "%{}"
    }

    lib.addStandardLibs()
--    lib.addNativeLib()
