@echo off
set SDL_INCLUDE=""
set SDL_LIB=""
cl -Zi /EHsc /MT /D"WIN32" /I"%SDL_INCLUDE%" "pianotutor.cpp" /link "SDL2.lib" /LIBPATH:"%SDL_LIB%" /OUT:"pianotutor.exe"