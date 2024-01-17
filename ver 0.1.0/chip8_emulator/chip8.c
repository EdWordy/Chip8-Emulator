// - - - - - - - - - - - - -
//   CHIP-8 EMULATOR v0.1.0
// - - - - - - - - - - - - -
// ------------------------

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "SDL.h"

// - - - - - - - - -
// ENUMS N STRUCTS
// - - - - - - - - -

typedef enum {
    QUIT,
    RUNNING,
    PAUSED
} emulator_state_t;

typedef struct {
    emulator_state_t state;
} chip8_t;

// - - - - - - - - -
// VARS
// - - - - - - - - -

int WINDOW_WIDTH = 64;                    // og values
int WINDOW_HEIGHT = 32;                   // og values
uint32_t FG_COLOUR = 0xFFFFFFFF;          // RGBA8888, WHITE
uint32_t BG_COLOUR = 0x00000000;          // RGBA8888, BLACK
int SCALE_FACTOR = 4;                     // 264 x 128 resolution

// - - - - - - - - -
// HELPER METHODS
// - - - - - - - - - 

bool init_sdl(SDL_Window *window, SDL_Renderer *renderer)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0)
    {
        SDL_Log("Unable to initalize SDL: %s", SDL_GetError());
        return false;
    }

    window = SDL_CreateWindow("CHIP8 EMULATOR", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH * SCALE_FACTOR, WINDOW_HEIGHT * SCALE_FACTOR, 0);
    if (!window)
    {
        SDL_Log("Unable to initalize SDL window: %s", SDL_GetError());
        return false;
    }
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer)
    {
        SDL_Log("Unable to initalize SDL renderer: %s", SDL_GetError());
        return false;
    }
    return true;
}

void final_sdl_cleanup(SDL_Window *window, SDL_Renderer *renderer)
{
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void clear_screen(SDL_Renderer *renderer)
{
    const uint8_t r = (BG_COLOUR >> 24) & 0xff;
    const uint8_t g = (BG_COLOUR >> 16) & 0xff;
    const uint8_t b = (BG_COLOUR >>  8) & 0xff;
    const uint8_t a = (BG_COLOUR >>  0) & 0xff;

    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    SDL_RenderClear(renderer);
}

void update_screen(SDL_Renderer *renderer)
{
    SDL_RenderPresent(renderer);
}

void handle_input(chip8_t *chip8)
{
    SDL_Event event;

    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
            case SDL_QUIT:
                // exit window; end program
                chip8->state = QUIT;
                return;
            // key down event
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym)
                {
                    case SDLK_ESCAPE:
                        // escape key: exit window and end program
                        chip8->state = QUIT;
                        return;
                    default:
                        break;
                }
                break;
            // key up event
            case SDL_KEYUP:
                break;
            default:
                break;
        }
    }
}

bool init_chip8(chip8_t *chip8)
{
    chip8->state = RUNNING;             // default machine state
    return true;
}

// - - - - - - - - -
// MAIN PROGRAM
// - - - - - - - - - 

int main (int argc, char **argv)
{
    // args
    (void)argc;
    (void)argv;

    // initialization
    // - - - - - - - -
    SDL_Window *window = 0;
    SDL_Renderer *renderer = 0;

    // setup
    if (!init_sdl(window, renderer)) exit(EXIT_FAILURE);
    chip8_t chip8 = {0};
    if (!init_chip8(&chip8)) exit(EXIT_FAILURE);

    // clear screen
    clear_screen(renderer);

    // main emulator loop
    // - - - - - - - -
    while (chip8.state != QUIT)
    {
        // handle input
        handle_input(&chip8);
        // if (chip8.state == PAUSED) continue;

        // get_time();
        // emulate chip8 insturctions 
        // here
        // get_time(); elapsed since last get_time();

        // delay for approx. 60hz/60fps (16.67ms)
        SDL_Delay(16);

        // update window with changes
        update_screen(renderer);
    }

    // cleanup
    // - - - - - - - -
    final_sdl_cleanup(window, renderer);

    // exit the program
    exit(EXIT_SUCCESS);
}