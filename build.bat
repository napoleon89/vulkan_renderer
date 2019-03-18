@echo off
set compiler_options=-MT -DDISABLE_VK_LAYER_VALVE_steam_overlay_1=1 -D_ITERATOR_DEBUG_LEVEL=0 -D_CRT_SECURE_NO_WARNINGS -W4 -FC -WX -Oi -Od -Gm- -GR- -Z7 -nologo -wd4312 -wd4806 -wd4701 -wd4505 -wd4201 -EHsc -wd4100 -wd4127 -wd4189 -wd4244 -wd4005 -Fo:..\temp\ -I..\deps\include -I..\src -I%VULKAN_SDK%\Include
set linker_options=-incremental:no -opt:ref -LIBPATH:../deps/lib/x64/windows/ -LIBPATH:%VULKAN_SDK%\Lib

if not exist build mkdir build
if not exist temp mkdir temp

pushd build
del *.pdb > NUL 2> NUL

xcopy ..\deps\bin\x64\*.dll . /Y /S /E /Q /D >NUL



cl %compiler_options% ../src/game/game.cpp -DPP_EDITOR -LD /link %linker_options% /DLL -PDB:game_%random%.pdb /EXPORT:gameInit /EXPORT:gameUpdate /EXPORT:gameRender 

cl %compiler_options% -Fe:engine22.exe -MP ../src/main.cpp ../src/core/platform/win32_platform.cpp  -Fm:wild.map /link %linker_options% user32.lib sdl2.lib sdl2main.lib soloud.lib vulkan-1.lib -SUBSYSTEM:CONSOLE 

popd

pushd data\shaders
%VULKAN_SDK%\Bin32\glslangValidator.exe -V ../../src/shaders/main.vert
%VULKAN_SDK%\Bin32\glslangValidator.exe -V ../../src/shaders/main.frag
popd