/*
    Copyright 2024 Arisotura

    This file is part of pomelopad.

    pomelopad is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    pomelopad is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with pomelopad. If not, see http://www.gnu.org/licenses/.
*/

#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>

#include "WUP.h"

using namespace std;

int main()
{
    SDL_Init(SDL_INIT_VIDEO);
    printf("pomelopad 0.1 or something\n");

    WUP::Init();
    if (!WUP::LoadFirmware("firmware.bin"))
    //if (!WUP::LoadFirmware("firmware_recent.bin"))
    {
        printf("failed to load firmware.bin\n");
        WUP::DeInit();
        SDL_Quit();
        return -1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "pomelopad",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        854, 480,
        0
    );

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer)
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);

    SDL_Texture* framebuf = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 854, 480);
    if (!framebuf)
    {
        printf("texture shat itself :(\n");
        return -1;
    }

    WUP::Start();

    bool quit = false;
    for (;;)
    {
        SDL_Event evt;
        while (SDL_PollEvent(&evt))
        {
            switch (evt.type)
            {
            case SDL_QUIT:
                quit = true;
                break;
            }
        }
        if (quit) break;

        // run emulation here
        WUP::RunFrame();

        {
            u32* src = WUP::GetFramebuffer();
            u8* dst;
            int stride;
            SDL_LockTexture(framebuf, nullptr, (void**)&dst, &stride);

            for (int y = 0; y < 480; y++)
            {
                memcpy(dst, src, 854*4);
                src += 854;
                dst += stride;
            }

            SDL_UnlockTexture(framebuf);
        }

        // redraw
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(renderer);

        SDL_RenderCopy(renderer, framebuf, nullptr, nullptr);

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(framebuf);
    SDL_DestroyRenderer(renderer);

    SDL_DestroyWindow(window);

    WUP::DeInit();

    SDL_Quit();
    return 0;
}
