@echo off
if not exist shaders\compiled mkdir shaders\compiled

C:\Libraries\VulkanSDK\1.4.321.1\Bin\glslc.exe shaders\simple.vert -o shaders\compiled\simple_vert.spv
C:\Libraries\VulkanSDK\1.4.321.1\Bin\glslc.exe shaders\simple.frag -o shaders\compiled\simple_frag.spv

pause