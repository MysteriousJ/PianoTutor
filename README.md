# Piano Tutor
Use this tool to analyze and optomize [pianoing](http://iplaywinner.com/glossary/inputs-technique/piano.html) inputs. Easily configure any sequence of button presses, then get feedback on frames between presses and button order.

It may be helpful to run this tool and play a game in a window at the same time, but keep in mind that drivers update to reflect joystick input state at random times. The game and Piano Tutor may receive inputs on different frames, and it seems that a 2-button plink is just not reliable on PC.

# Dependencies
SDL (SDL2.dll is included in the repo)

# Building
Windows: edit build.bat with your directories for SDL header and lib files. Run build.bat from a visual studio command line.
OSX/Linux: should work, but I haven't tested it. The code is "cross platform," but this is C++ so that doesn't mean much.

# License
MIT
