@echo off
for %%f in (shaders\*.vert) do "%VULKAN_SDK%\Bin\glslc.exe" "%%f" -o "%%f.spv"
for %%f in (shaders\*.frag) do "%VULKAN_SDK%\Bin\glslc.exe" "%%f" -o "%%f.spv"