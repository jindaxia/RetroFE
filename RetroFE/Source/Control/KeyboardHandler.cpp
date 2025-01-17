#include "KeyboardHandler.h"

KeyboardHandler::KeyboardHandler(SDLKey s)
: scancode_(s)
, pressed_(false)
{
}

void KeyboardHandler::reset()
{
    pressed_= false;
}

bool KeyboardHandler::update(SDL_Event &e)
{
    if(e.type != SDL_KEYUP && e.type != SDL_KEYDOWN) return false;

    if(e.key.keysym.sym == scancode_)
    {
        pressed_ = (e.type == SDL_KEYDOWN);
        return true;
    }

    return false;
}

bool KeyboardHandler::pressed()
{
    return pressed_;
}

