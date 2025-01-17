/* This file is part of RetroFE.
 *
 * RetroFE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * RetroFE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with RetroFE.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <string>
#include <SDL/SDL_mixer.h>
#include "SDL.h"
class Sound
{
public:
    Sound(std::string file, std::string altfile);
    virtual ~Sound();
    void play();
    bool allocate();
    bool free();
    bool isPlaying();
private:
    static void finished(int channel);
    static uint32_t turnOffAmpli(uint32_t interval, void *param);
    static int ampliStarted;
    static SDL_TimerID idTimer;
    std::string file_;
    Mix_Chunk  *chunk_;
    int         channel_;
};
