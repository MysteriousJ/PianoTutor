@echo off
set SDL_INCLUDE="E:\Everything\Files\Code\sdl\include"
set SDL_LIB="E:\Everything\Files\Code\sdl\lib"
cl -Zi /EHsc /MT /D"WIN32" /I"%SDL_INCLUDE%" "pianotutor.cpp" /link "SDL2.lib" /LIBPATH:"%SDL_LIB%" /OUT:"pianotutor.exe"