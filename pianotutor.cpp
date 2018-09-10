#include <stdio.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include "SDL/SDL.h"
#include "SDL/SDL_joystick.h"
#include "SDL/SDL_keyboard.h"
#undef main // Get rid of SDL_main

#define forloop(i,end) for(unsigned int i=0; i<(end); i++)
typedef unsigned int uint;

enum Mode
{
	Mode_setSequence,
	Mode_practice
};

struct DisplayText
{
	std::wstring intro;
	std::wstring config;
	std::wstring practice;
};

struct ButtonInputAction
{
	uint buttonIndex;
	uint joystickIndex;
};

struct HatInputAction
{
	uint pov;
	uint joystickIndex;
};

struct KeyboardInputAction
{
	uint keyIndex;
};

struct InputAction
{
	enum Type { Type_button, Type_hat, Type_keyboard };

	union {
		ButtonInputAction button;
		HatInputAction hat;
		KeyboardInputAction key;
	};

	Type type;
};

struct PianoState
{
	std::vector<InputAction> pianoSequence;
	uint frameCount = 0;
	uint lastInputFrame = 0;
	uint nextExpectedPianoIndex = 0;
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
		int hat;
		int previousHat;
		SDL_Joystick* sdlJoy;
	};

	static const uint supportedKeyCount = 0xFF;
	Button keyboard[supportedKeyCount];
	uint joystickCount;
	Joystick* joysticks;
};

std::wstring parseString(std::wifstream& inStream)
{
	std::wstringstream outStream;
	wchar_t c;
	// Backslash delimited
	while (inStream >> std::noskipws >> c && c != L'\\') outStream << c;
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
		forloop (i, input->joystickCount)
		{
			delete[] input->joysticks[i].buttons;
			SDL_JoystickClose(input->joysticks[i].sdlJoy);
		}
		delete[] input->joysticks;
		// Allocate joysticks
		input->joystickCount = joystickCount;
		input->joysticks = new Input::Joystick[joystickCount];
		forloop (i, input->joystickCount)
		{
			input->joysticks[i].sdlJoy = SDL_JoystickOpen(i);
			if (input->joysticks[i].sdlJoy)
			{
				input->joysticks[i].buttonCount = SDL_JoystickNumButtons(input->joysticks[i].sdlJoy);
				input->joysticks[i].buttons = new Input::Button[input->joysticks[i].buttonCount];
			}
		}
	}

	// Update joystick
	SDL_JoystickUpdate();
	forloop (joystickIndex, input->joystickCount)
	{
		Input::Joystick* joystick = &input->joysticks[joystickIndex];
		// Buttons
		forloop (buttonIndex, joystick->buttonCount)
		{
			updateButton(&joystick->buttons[buttonIndex], SDL_JoystickGetButton(joystick->sdlJoy, buttonIndex));
		}
		// Hat
		joystick->previousHat = joystick->hat;
		joystick->hat = SDL_JoystickGetHat(joystick->sdlJoy, 0);
	}
	// Update keyboard
	int sdlKeyCount;
	const Uint8 *keystates = SDL_GetKeyboardState(&sdlKeyCount);
	forloop(i, Input::supportedKeyCount)
	{
		int isKeyDown = 0;
		// Make sure we don't read past the end of SDL's key array
		if (i < (uint)sdlKeyCount) {
			isKeyDown = keystates[i];
		}
		updateButton(&input->keyboard[i], isKeyDown);
	}
}

bool compareInputActions(InputAction a, InputAction b)
{
	if (a.type != b.type) return false;
	switch (a.type)
	{
	case InputAction::Type_button:
		return a.button.buttonIndex == b.button.buttonIndex
			&& a.button.joystickIndex == b.button.joystickIndex;
	case InputAction::Type_hat:
		return a.hat.pov == b.hat.pov
			&& a.hat.joystickIndex == b.hat.joystickIndex;
	case InputAction::Type_keyboard:
		return a.key.keyIndex == b.key.keyIndex;
	default:
		SDL_assert(!"Unimplemented input action type");
		return false;
	}
}

std::vector<InputAction> getActiveInputsList(Input input)
{
	std::vector<InputAction> list;

	forloop(joystickIndex, input.joystickCount)
	{
		forloop(buttonIndex, input.joysticks[joystickIndex].buttonCount)
		{
			if (input.joysticks[joystickIndex].buttons[buttonIndex].pressed)
			{
				InputAction action;
				action.type = InputAction::Type_button;
				action.button.joystickIndex = joystickIndex;
				action.button.buttonIndex = buttonIndex;
				list.push_back(action);
			}
		}

		if (input.joysticks[joystickIndex].hat != input.joysticks[joystickIndex].previousHat)
		{
			InputAction action;
			action.type = InputAction::Type_hat;
			action.hat.joystickIndex = joystickIndex;
			action.hat.pov = input.joysticks[joystickIndex].hat;
			list.push_back(action);
		}
	}

	forloop(keyIndex, input.supportedKeyCount)
	{
		if (input.keyboard[keyIndex].pressed)
		{
			if (keyIndex != SDL_SCANCODE_RETURN && keyIndex != SDL_SCANCODE_BACKSPACE)
			{
				InputAction action;
				action.type = InputAction::Type_keyboard;
				action.key.keyIndex = keyIndex;
				list.push_back(action);
			}
		}
	}

	return list;
}

void printPracticeLetter(PianoState* state, InputAction input)
{
	// If the sequence contains more than one of the same input,
	// print the letter for the one that's next.
	// If there isn't one next, print the closest one.
	// If the sequence doesn't contain the input, print a dash.
	char inputLetter = '-';
	forloop(i, state->pianoSequence.size())
	{
		if (compareInputActions(state->pianoSequence[i], input))
		{
			inputLetter = 'a'+i;
			if (i >= state->nextExpectedPianoIndex) break;
		}
	}
	printf("%c", inputLetter);
}

void printSetSequenceInput(InputAction input, uint index)
{
	if (input.type == InputAction::Type_button)
	{
		printf("Joystick %d, button %d (%c)\n", input.button.joystickIndex, input.button.buttonIndex, 'a'+index);
	}
	if (input.type == InputAction::Type_hat)
	{
		// Print the hat number in numpad notation
		char hatChar = ' ';
		switch (input.hat.pov) {
			case SDL_HAT_CENTERED:  hatChar = '5'; break;
			case SDL_HAT_RIGHT:     hatChar = '6'; break;
			case SDL_HAT_RIGHTDOWN: hatChar = '3'; break;
			case SDL_HAT_DOWN:      hatChar = '2'; break;
			case SDL_HAT_LEFTDOWN:  hatChar = '1'; break;
			case SDL_HAT_LEFT:      hatChar = '4'; break;
			case SDL_HAT_LEFTUP:    hatChar = '7'; break;
			case SDL_HAT_UP:        hatChar = '8'; break;
			case SDL_HAT_RIGHTUP:   hatChar = '9'; break;
		}
		printf("Joystick %d, hat %c (%c)\n", input.hat.joystickIndex, hatChar, 'a'+index);
	}
	if (input.type == InputAction::Type_keyboard)
	{
		const char* keyName = SDL_GetKeyName(SDL_GetKeyFromScancode((SDL_Scancode)input.key.keyIndex));
		printf("%s key (%c)\n", keyName, 'a'+index);
	}
}

void updateSetSequence(PianoState* state, Input input)
{
	std::vector<InputAction> currentInputList = getActiveInputsList(input);

	forloop(i, currentInputList.size())
	{
		printSetSequenceInput(currentInputList[i], state->pianoSequence.size());
		state->pianoSequence.push_back(currentInputList[i]);
	}
}

void updatePractice(PianoState* state, Input input)
{
	std::vector<InputAction> currentInputList = getActiveInputsList(input);

	forloop (i, currentInputList.size())
	{
		if (state->nextExpectedPianoIndex < state->pianoSequence.size())
		{
			if (state->lastInputFrame != 0)
			{
				uint frameDelta = state->frameCount - state->lastInputFrame;
				printf(" %d ", frameDelta);
				if (frameDelta == 0) printf(" MISS ");
			}

			printPracticeLetter(state, currentInputList[i]);

			if (compareInputActions(state->pianoSequence[state->nextExpectedPianoIndex], currentInputList[i]))
			{
				state->nextExpectedPianoIndex += 1;
			}
			else printf(" MISS ");
			state->lastInputFrame = state->frameCount;
		}
	}

	if (state->lastInputFrame != 0)
	{
		uint frameDelta = state->frameCount - state->lastInputFrame;
		if (frameDelta >= 15 || state->nextExpectedPianoIndex >= state->pianoSequence.size())
		{
			state->frameCount = 0;
			state->lastInputFrame = 0;
			state->nextExpectedPianoIndex = 0;
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
	
	Mode mode = Mode_setSequence;

	PianoState state;

	// Load localized display text from a file
	DisplayText text = parseTextFile("en.txt");

	std::wcout << text.intro;
	std::wcout << text.config;

	bool run = true;
	while (run)
	{
		bool enterPressed = false;
		bool backspacePressed = false;

		// Process window messages
		SDL_Event message;
		while (SDL_PollEvent(&message)) {
			if (message.type == SDL_QUIT) run = false;
			if (message.type == SDL_KEYDOWN) {
				if (message.key.keysym.sym == SDLK_RETURN) enterPressed = true;
				if (message.key.keysym.sym == SDLK_BACKSPACE) backspacePressed = true;
			}
		}

		updateInput(&input);

		if (mode == Mode_setSequence)
		{
			updateSetSequence(&state, input);

			if (enterPressed && state.pianoSequence.size() > 0)
			{
				std::wcout << text.practice;
				mode = Mode_practice;
			}
		}
		else if (mode == Mode_practice)
		{
			updatePractice(&state, input);
		}
		
		if (backspacePressed && state.pianoSequence.size() > 0)
		{
			std::wcout << text.config;
			mode = Mode_setSequence;
			state.pianoSequence.clear();
			state.nextExpectedPianoIndex = 0;
		}
		
		++state.frameCount;
		fflush(stdout);
		SDL_GL_SetSwapInterval(1); // Use vsynch
		SDL_GL_SwapWindow(window);
	}

	return 0;
}