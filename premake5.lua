workspace "VCT_Experimental"
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
	IncDir["assimp"] = "3rdparty/assimp/include"
	IncDir["assimp_build"] = "3rdparty/assimp/build/include"
	IncDir["stb"] = "3rdparty/stb"

    group "3rdparty"
    include "3rdparty/glfw"
    include "3rdparty/glad"
    include "3rdparty/imgui"
    externalproject "assimp"
        location "3rdparty/assimp/build/code"
        kind "StaticLib"
        language "C++"
        configmap {
            ["Debug"] = "Debug",
            ["Release"] = "MinSizeRel"
        }
    group ""

    project "VCT_Experimental"
        kind "ConsoleApp"
        language "C++"
        cppdialect "C++17"
        targetdir "bin/%{prj.name}/%{cfg.buildcfg}"
        objdir "obj/%{prj.name}/%{cfg.buildcfg}"

        links {
			"assimp",
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
			"%{IncDir.assimp}",
			"%{IncDir.assimp_build}",
			"%{IncDir.stb}",
            "src"
        }

        defines {

        }

        files { 
            "src/**.cpp",
            "src/**.hpp"
        }

        filter "system:windows"
            systemversion "latest"
            disablewarnings { 26812 }
			
        filter "configurations:Debug"
            symbols "on"

        filter "configurations:Release"
            optimize "on"