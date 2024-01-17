// - - - - - - - - - - - - -
//   CHIP-8 EMULATOR v0.2.0
// - - - - - - - - - - - - -
// ------------------------

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "SDL.h"

// - - - - - - - - -
// ENUMS N STRUCTS
// - - - - - - - - -

// state obj
typedef enum {
    QUIT,
    RUNNING,
    PAUSED
} emulator_state_t;

// chip8 machine obj
typedef struct {
    emulator_state_t state;
    uint8_t ram[4096];
    uint8_t display[64*32];               // if using pointers: display == &ram[0xF00]; e.g the upper most 256 bits of ram
    uint16_t stack[12];                   // subroutine stack
    uint8_t V[16];                        // Data registers V0-VF
    uint16_t I;                           // Index register
    uint16_t PC;                          // Program counter
    uint8_t delay_timer;                  // Decrements at 60hz when >0
    uint8_t sound_timer;                  // Decrements at 60hz and plays tone when >0
    bool keypad[16];                      // Hexadeicaml Keypad 0x0-0xF
    char *rom_name;                       // Currently running ROM
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

                    case SDLK_SPACE:
                        // pause key: for debugging
                        if (chip8->state == RUNNING)
                        {
                            chip8->state = PAUSED;      // Pause
                            puts("==== PAUSED =====");
                        }
                        else
                        {
                            chip8->state = RUNNING;    // Resume
                        }
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

bool init_chip8(chip8_t *chip8, char *rom_name)
{
    const uint32_t entry_point = 0x200;  // CHIP8 Roms will be loaded to 0x200

    const uint8_t font[] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0,    // 0
        0x20, 0x60, 0x20, 0x20, 0x70,    // 1  
        0xF0, 0x10, 0xF0, 0x80, 0xF0,    // 2
        0xF0, 0x10, 0xF0, 0x10, 0xF0,    // 3
        0x90, 0x90, 0xF0, 0x10, 0x10,    // 4
        0xF0, 0x80, 0xF0, 0x10, 0xF0,    // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0,    // 6
        0xF0, 0x10, 0x20, 0x40, 0x40,    // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0,    // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0,    // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90,    // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0,    // B
        0xF0, 0x80, 0x80, 0x80, 0xF0,    // C
        0xE0, 0x90, 0x90, 0x90, 0xE0,    // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0,    // E
        0xF0, 0x80, 0xF0, 0x80, 0x80     // F
    };

    // Load font
    memcpy(&chip8->ram[0], font, sizeof(font));

    // open ROM file
    FILE *rom = fopen(rom_name, "rb");
    if (!rom)
    {
        SDL_Log("Rom file %s is invalid or does not exist\n", rom_name);
        return false;
    }

    // get and check ROM size
    fseek(rom, 0, SEEK_END);
    const size_t rom_size = ftell(rom);
    const size_t max_size = sizeof(chip8->ram - entry_point);
    rewind(rom);

    // size check
    if (rom_size > max_size)
    {
        SDL_Log("ROM file %s is too large. ROM size: %d, Max Size allowed: %d\n", rom_name, (int)rom_size, (int)max_size);
        return false;
    }

    // Load ROM
    if (fread(&chip8->ram[entry_point], rom_size, 1, rom) != 1)
    {
        SDL_Log("Could not read ROM file %s into CHIP8 memory\n", rom_name);
        return false;
    }

    // ROM cleanup
    fclose(rom);

    // set chip8 machine defaults
    chip8->state = RUNNING;             // default machine state
    chip8->PC = entry_point;            // start program counter
    chip8->rom_name = rom_name;

    return true;
}

// - - - - - - - - -
// MAIN PROGRAM
// - - - - - - - - - 

int main (int argc, char **argv)
{
    // args
    (void)argc;

    // initialization
    // - - - - - - - -
    SDL_Window *window = 0;
    SDL_Renderer *renderer = 0;

    // setup
    if (!init_sdl(window, renderer)) exit(EXIT_FAILURE);
    chip8_t chip8 = {0};
    if (!init_chip8(&chip8, argv[1])) exit(EXIT_FAILURE);

    // clear screen
    clear_screen(renderer);

    // main emulator loop
    // - - - - - - - -
    while (chip8.state != QUIT)
    {
        // handle input
        handle_input(&chip8);

        if (chip8.state == PAUSED) continue;

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