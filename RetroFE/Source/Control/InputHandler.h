#pragma once

#include <SDL/SDL.h>

class InputHandler
{
public:
    virtual ~InputHandler() {};
    virtual bool update(SDL_Event &e) = 0;
    virtual bool pressed() = 0;
    virtual void reset() = 0;
};

