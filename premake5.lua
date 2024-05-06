workspace "Template"
    architecture "x64"
    configurations { 
        "Debug", 
        "Release" 
    }

    IncDir = {}
    IncDir["glfw"] = "3rdparty/glfw/include"
    IncDir["glad"] = "3rdparty/glad/include"
    IncDir["imgui"] = "3rdparty/imgui"
    IncDir["glm"] = "3rdparty/glm"

    group "3rdparty"
    include "3rdparty/glfw"
    include "3rdparty/glad"
    include "3rdparty/imgui"
    group ""

    project "Template"
        kind "ConsoleApp"
        language "C++"
        cppdialect "C++17"
        targetdir "bin/%{prj.name}/%{cfg.buildcfg}"
        objdir "obj/%{prj.name}/%{cfg.buildcfg}"
        vectorextensions "AVX2"

        links {
            "glad",
            "glfw",
            "imgui",
            "opengl32.lib"
        }

        includedirs {
            "%{IncDir.glfw}",
            "%{IncDir.glad}",
            "%{IncDir.imgui}",
            "%{IncDir.glm}",
            "src"
        }

        defines {

        }

        files { 
            "src/**.cpp",
            "src/**.hpp"
        }

        filter "system:windows"
            staticruntime "on"
            systemversion "latest"
            disablewarnings {
                "26812"
            }

        filter "configurations:Debug"
            symbols "on"

        filter "configurations:Release"
            optimize "on"