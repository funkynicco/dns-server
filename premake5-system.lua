lib = {}

lib.addStandardLibs = function(options)
    filter { "kind:not StaticLib", "system:windows" }
        links {
            "ws2_32.lib",
        }

    --filter { "kind:not StaticLib", "system:linux" }
    --    links {
    --        "potato.lib",
    --    }

    filter {}
end

lib.addNativeLib = function(options)
    if not options or not options.no_include then
        includedirs {
            "../libraries/nativelib/include"
        }
    end

    filter "kind:not StaticLib"
        libdirs {
            "%{wks.location}/libraries/nativelib/lib/vs2022/x64"
        }

        links {
            "nativelib_d.lib",
        }

    filter {}
end