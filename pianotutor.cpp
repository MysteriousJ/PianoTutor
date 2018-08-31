#include <stdio.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include "SDL/SDL_main.h"
#include "SDL/SDL.h"
#include "SDL/SDL_joystick.h"
#include "SDL/SDL_keyboard.h"
#undef main // Get rid of SDL_main

typedef unsigned int uint;

enum Mode
{
	Mode_setJoystick,
	Mode_setSequence,
	Mode_practice
};

struct DisplayText
{
	std::wstring intro;
	std::wstring config;
	std::wstring practice;
};

struct PianoState
{
	std::vector<uint> buttonSequence;
	uint frameCount = 0;
	uint lastInputFrame = 0;
	uint nextExpectedButtonIndex = 0;
};

struct Input
{
	struct Button {
		bool pressed = 0; // True for one update when the button is first pressed.
		bool down = 0;    // True while the button is held down.
	};
	struct Joystick {
		uint buttonCount;
		Button *buttons;
		SDL_Joystick* sdlJoy;
	};

	uint joystickCount;
	Joystick* joysticks;
};

std::wstring parseString(std::wifstream& inStream)
{
	std::wstringstream outStream;
	wchar_t c;
	// Backslash delimited
	while (inStream >> std::noskipws >> c && c != L'\\')
	{
		outStream << c;
	}
	return outStream.str();
}

DisplayText parseTextFile(std::string fileName)
{
	DisplayText result;
	std::wifstream input(fileName);
	result.intro = parseString(input);
	result.config = parseString(input);
	result.practice = parseString(input);
	return result;
}

void updateButton(Input::Button* inout_button, uint isDown)
{
	if (isDown) {
		inout_button->pressed = !inout_button->down;
		inout_button->down = true;
	}
	else {
		inout_button->pressed = false;
		inout_button->down = false;
	}
}

void updateInput(Input* input)
{
	// Handle joysticks beening plugged in or taken out
	int joystickCount = SDL_NumJoysticks();
	if (joystickCount != input->joystickCount)
	{
		// Free joysticks
		for (uint i=0; i<input->joystickCount; ++i)
		{
			delete[] input->joysticks[i].buttons;
			SDL_JoystickClose(input->joysticks[i].sdlJoy);
		}
		delete[] input->joysticks;
		// Allocate joysticks
		input->joystickCount = joystickCount;
		input->joysticks = new Input::Joystick[joystickCount];
		for (uint i=0; i<input->joystickCount; ++i)
		{
			input->joysticks[i].sdlJoy = SDL_JoystickOpen(i);
			if (input->joysticks[i].sdlJoy)
			{
				input->joysticks[i].buttonCount = SDL_JoystickNumButtons(input->joysticks[i].sdlJoy);
				input->joysticks[i].buttons = new Input::Button[input->joysticks[i].buttonCount];
			}
		}
	}

	// Update Joystick
	SDL_JoystickUpdate();
	for (uint joystickIndex=0; joystickIndex<input->joystickCount; ++joystickIndex)
	{
		// Buttons
		for (uint buttonIndex=0; buttonIndex<input->joysticks[joystickIndex].buttonCount; ++buttonIndex)
		{
			updateButton(&input->joysticks[joystickIndex].buttons[buttonIndex], SDL_JoystickGetButton(input->joysticks[joystickIndex].sdlJoy, buttonIndex));
		}
	}
}

bool anyButtonPressed(Input::Joystick joystick)
{
	for (uint i=0; i<joystick.buttonCount; ++i)
	{
		if (joystick.buttons[i].pressed) return true;
	}
	return false;
}

void updateSetSequence(PianoState* state, Input::Joystick joystick)
{
	for (uint i=0; i<joystick.buttonCount; ++i)
	{
		if (joystick.buttons[i].pressed)
		{
			printf("Button %d (%c)\n", i, 'a'+state->buttonSequence.size());
			state->buttonSequence.push_back(i);
		}
	}
}

void updatePractice(PianoState* state, Input::Joystick joystick)
{
	for (uint i=0; i<joystick.buttonCount; ++i)
	{
		if (joystick.buttons[i].pressed && state->nextExpectedButtonIndex < state->buttonSequence.size())
		{
			if (state->lastInputFrame != 0)
			{
				uint frameDelta = state->frameCount - state->lastInputFrame;
				printf(" %d ", frameDelta);
				if (frameDelta == 0) printf(" MISS ");
			}

			bool buttonInSequence = false;
			for (uint j=0; j<state->buttonSequence.size(); ++j)
			{
				if (state->buttonSequence[j] == i)
				{
					buttonInSequence = true;
					printf("%c", 'a'+j);
				}
			}
			if (!buttonInSequence) printf("-");

			if (i == state->buttonSequence[state->nextExpectedButtonIndex]) state->nextExpectedButtonIndex += 1;
			else printf(" MISS ");
			state->lastInputFrame = state->frameCount;
		}
	}

	if (state->lastInputFrame != 0)
	{
		uint frameDelta = state->frameCount - state->lastInputFrame;
		if (frameDelta >= 15 || state->nextExpectedButtonIndex >= state->buttonSequence.size())
		{
			state->frameCount = 0;
			state->lastInputFrame = 0;
			state->nextExpectedButtonIndex = 0;
			printf("\n");
		}
	}
}

int main(int argc, char** argv)
{
	// Needs a window for keyboard input and vsynch
	SDL_Init(SDL_INIT_VIDEO);
	SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
	SDL_Window* window = SDL_CreateWindow(0, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 250, 0, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	SDL_GLContext graphicsContext = SDL_GL_CreateContext(window);
	SDL_SetWindowTitle(window, "Input Capture");

	Input input = {0};
	updateInput(&input);
	
	Mode mode = Mode_setJoystick;
	uint joystickIndex = 0;

	PianoState state;

	// Load localized display text from a file
	DisplayText text = parseTextFile("en.txt");

	std::wcout << text.intro;

	bool run = true;
	while (run)
	{
		bool enterPressed = false;
		bool backspacePressed = false;

		// Process window messges
		SDL_Event message;
		while (SDL_PollEvent(&message)) {
			if (message.type == SDL_QUIT) run = false;
			if (message.type == SDL_KEYDOWN) {
				if (message.key.keysym.sym == SDLK_RETURN) enterPressed = true;
				if (message.key.keysym.sym == SDLK_BACKSPACE) backspacePressed = true;
			}
		}

		updateInput(&input);

		if (mode == Mode_setJoystick)
		{
			for (uint i=0; i<input.joystickCount; ++i)
			{
				if (anyButtonPressed(input.joysticks[i]))
				{
					joystickIndex = i;
					mode = Mode_setSequence;
				}
			}
			if (mode == Mode_setSequence) std::wcout << text.config;;
		}
		else if (joystickIndex < input.joystickCount) // Make sure the joystick is still there
		{
			if (mode == Mode_setSequence)
			{
				updateSetSequence(&state, input.joysticks[joystickIndex]);

				if (enterPressed && state.buttonSequence.size() > 0)
				{
					std::wcout << text.practice;
					mode = Mode_practice;
				}
			}
			else if (mode == Mode_practice)
			{
				updatePractice(&state, input.joysticks[joystickIndex]);
			}
		}

		if (mode == Mode_setSequence || mode == Mode_practice)
		{
			if (backspacePressed)
			{
				mode = Mode_setSequence;
				state.buttonSequence.clear();
				state.nextExpectedButtonIndex = 0;
				std::wcout << text.config;
			}
		}

		++state.frameCount;
		fflush(stdout);
		SDL_GL_SetSwapInterval(1); // Use vsynch
		SDL_GL_SwapWindow(window);
	}

	return 0;
}