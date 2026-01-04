@echo off
if not exist shaders\compiled mkdir shaders\compiled

C:\Libraries\VulkanSDK\1.4.321.1\Bin\glslc.exe shaders\simple.vert -o shaders\compiled\simple.vert.spv
C:\Libraries\VulkanSDK\1.4.321.1\Bin\glslc.exe shaders\simple.frag -o shaders\compiled\simple.frag.spv
C:\Libraries\VulkanSDK\1.4.321.1\Bin\glslc.exe shaders\particle.comp -o shaders\compiled\particle.comp.spv
C:\Libraries\VulkanSDK\1.4.321.1\Bin\glslc.exe shaders\particle.vert -o shaders\compiled\particle.vert.spv
C:\Libraries\VulkanSDK\1.4.321.1\Bin\glslc.exe shaders\particle.frag -o shaders\compiled\particle.frag.spv
echo Shader compilation successfully.

pause