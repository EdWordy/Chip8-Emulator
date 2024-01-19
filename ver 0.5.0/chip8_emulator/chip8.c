// - - - - - - - - - - - - -
//   CHIP-8 EMULATOR v0.4.0
// - - - - - - - - - - - - -
// ------------------------

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

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

// chip8 instruction format
typedef struct {
    uint16_t opcode;                      // Opcode name
    uint16_t NNN;                         // 12 bit address/constant
    uint8_t NN;                           // 8 bit constant
    uint8_t N;                            // 4 bit const
    uint8_t X;                            // 4 bit register id                   
    uint8_t Y;                            // 4 bit register id           
} instruction_t;

// chip8 machine obj
typedef struct {
    emulator_state_t state;
    uint8_t ram[4096];
    uint8_t display[64*32];               // if using pointers: display == &ram[0xF00]; e.g the upper most 256 bits of ram
    uint16_t stack[12];                   // Subroutine stack
    uint16_t *stack_ptr;                  // Stack pointer
    uint8_t V[16];                        // Data registers V0 to VF
    uint16_t I;                           // Index register
    uint16_t PC;                          // Program counter
    uint8_t delay_timer;                  // Decrements at 60hz when >0
    uint8_t sound_timer;                  // Decrements at 60hz and plays tone when >0
    bool keypad[16];                      // Hexadeicaml Keypad 0x0 to 0xF
    char *rom_name;                       // Currently running ROM
    instruction_t inst;                   // Currently executing Instruction
} chip8_t;

// - - - - - - - -
// IFDEF
// - - - - - - - -

#ifdef DEBUG
void print_debug_info(chip8_t *chip8)
{
    printf("Address: 0x%04X, Opcode: 0x%04X, Desc: ", chip8->PC - 2, chip8->inst.opcode);

    switch ((chip8->inst.opcode >> 12) & 0x0F)
    {
        case 0x00:                       // if the top 12 bits are zero
            if (chip8->inst.NN == 0xE0)  // if the lowest 12 are
            {
                // 0x00E: Clear the screen
                printf("Clear the screen.\n");
            }
            else if (chip8->inst.NN == 0xEE)
            {   
                // 0x00EE: Return from subroutine
                printf("Return from subroutine to address 0x%04X.\n", *(chip8->stack_ptr - 1));
            } 
            else
            {
                printf("Unimplemented Opcode.\n");
            }
            break;

        case 0x01:
            // 0x1NNN: Jump to address NNN
            printf("Jump to address NNN (0x%02X)\n", chip8->inst.NNN);
            break;

        case 0x02:
            // 0x2NNN: Call subroutine at NNN
            break;

        case 0x03:
            // 0x3XNN: Check if VX == NN, if so, skip the next instruction
            printf("Check if V%X (0x%02X) == NN (0x%02X), skip next instruction if true.\n", chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.NN);
            break;

         case 0x04:
            // 0x4XNN: Check if VX != NN, if so, skip the next instruction
            printf("Check if V%X (0x%02X) != NN (0x%02X), skip next instruction if true.\n", chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.NN);
            break;
            
        case 0x05:
            // 0x5XY0: Check if VX == VY, if so, skip next instruction
            printf("Check if V%X (0x%02X) == V%X (0x%02X), skip next instruction if true.\n", chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.Y, chip8->V[chip8->inst.Y]);
            break;

        case 0x06:
            // 0x6XNN: Set register VX to NN
            // V offset by X
            printf("Set register V%X to NN (0x%02X)\n", chip8->inst.X, chip8->inst.NN);
            break;

        case 0x07:
            // 0x7XNN: Set register VX += NN
            printf("Set register V%X (0x%02x) += NN (0x%02X). Result: 0x%02X\n", chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.NN, chip8->V[chip8->inst.X] + chip8->inst.NN);
            break;

        case 0x08:
            switch (chip8->inst.N)
            {
                case 0:
                    // 0x8XY0: Set register VX = VY
                    printf("Set register V%X = V%X (0x%02X)\n", chip8->inst.X, chip8->inst.Y, chip8->V[chip8->inst.Y]);
                    break;

                case 1:
                    // 0x8XY1: Set register VX |= VY
                    printf("Set register V%X (0x%02X) |= V%X (0x%02X). Result: 0x%02X\n", chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.Y, chip8->V[chip8->inst.Y], chip8->V[chip8->inst.X] | chip8->V[chip8->inst.Y]); 
                    break;

                case 2:
                    // 0x8XY2: Set register VX &= VY
                    printf("Set register V%X (0x%02X) &= V%X (0x%02X). Result: 0x%02X\n", chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.Y, chip8->V[chip8->inst.Y], chip8->V[chip8->inst.X] & chip8->V[chip8->inst.Y]); 
                    break;

                case 3:
                    // 0x8XY3: Set register VX ^= VY
                    printf("Set register V%X (0x%02X) ^= V%X (0x%02X). Result: 0x%02X\n", chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.Y, chip8->V[chip8->inst.Y], chip8->V[chip8->inst.X] ^ chip8->V[chip8->inst.Y]); 
                    break;

                case 4:
                    // 0x8XY4: Set register VX += VY, set VF to 1 if carry, 0 if not
                    printf("Set register V%X (0x%02X) += V%X (0x%02X), VF = 1 if carry; Result: 0x%02X, VF = %X\n", chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.Y, chip8->V[chip8->inst.Y], chip8->V[chip8->inst.X] + chip8->V[chip8->inst.Y], ((uint16_t)(chip8->V[chip8->inst.X] + chip8->V[chip8->inst.Y]) > 255));
                    break;

                case 5:
                    // 0x8XY5: Set register VX -= VY, set VF to 1 if there is not a borrow (result is positive)
                    printf("Set register V%X (0x%02X) -= V%X (0x%02X), VF = 1 if no borrow; Result: 0x%02X, VF = %X\n", chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.Y, chip8->V[chip8->inst.Y], chip8->V[chip8->inst.X] - chip8->V[chip8->inst.Y], (chip8->V[chip8->inst.Y] <= chip8->V[chip8->inst.X]));
                    break;

                case 6:
                    // 0x8XY6: Set register VX >>= 1, store shifted off bit in VF
                    printf("Set register V%X (0x%02X) >>= 1, VF = shifted off bit (%X); Result: 0x%02X\n", chip8->inst.X, chip8->V[chip8->inst.X], chip8->V[chip8->inst.X] & 1, chip8->V[chip8->inst.X] >> 1);
                    break;

                case 7:
                    // 0x8XY7: Set register VX = VY - VX. set VF to 1 if there is not a borrow (result is positive)
                    printf("Set register V%X = V%X (0x%02X) - V%X (0x%02X), VF = 1 if no borrow; Result: 0x%02X, VF = %X\n", chip8->inst.X, chip8->inst.Y, chip8->V[chip8->inst.Y], chip8->inst.X, chip8->V[chip8->inst.X], chip8->V[chip8->inst.Y] - chip8->V[chip8->inst.X], (chip8->V[chip8->inst.X] <= chip8->V[chip8->inst.Y]));
                    break;

                case 0xE:
                    // 0x8XY8: Set register VX <<= 1, store shifted off bit in VF
                    printf("Set register V%X (0x%02X) <<= 1, VF = shifted off bit (%X); Result: 0x%02X\n", chip8->inst.X, chip8->V[chip8->inst.X], (chip8->V[chip8->inst.X] & 0x80) >> 7, chip8->V[chip8->inst.X] << 1);
                    break;

                default:
                    // wrong/unimplemented opcode
                    break;
            }
            break;

        case 0x09:
            // 0x9XY0: Check if VX != VY; skip next instruction if so
            printf("Check if V%X (0x%02X) != V%X (0x%02X), skip next instruction if true.\n", chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.Y, chip8->V[chip8->inst.Y]);
            break;

        case 0x0A:
            // 0XANNN: Set index register I to NNN
            printf("Set I to NNN (0x%04X).\n", chip8->inst.NNN);
            break;

        case 0x0B:
            // 0xBNNN: Jump to V0 + NNN
            printf("Set PC to V0 (0x%02X) + NNN (0x%04X). Result PC = 0x%04X\n", chip8->V[0], chip8->inst.NNN, chip8->V[0] + chip8->inst.NNN);
            break;

        case 0x0C:
            // 0xCXNN: Set register VX = rand() % 256 & NN (bitwise AND)
            printf("Set V%X = (rand() %% 256) & NN (0x%02x)\n", chip8->inst.X, chip8->inst.NN);
            break;

        case 0x0D:
            // 0xDXYN: Draw N-height sprite at coordinate X,Y
            printf("Draw N (%u) height sprite at coords V%X (0x%02X), V%X (0x%02X) from memory location I (0x%04X). Set VF = 1 if any pixels are turned off.\n", chip8->inst.N, chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.Y, chip8->V[chip8->inst.Y], chip8->I);
            break;

        case 0x0E:
            if (chip8->inst.NN == 0x09E)
            {
                // 0xEX9E: Skip next instruction if key in VX is pressed
                printf("Skip next instruction if key in V%X (0x%02X) is pressed. Keypad value:  %d\n", chip8->inst.X, chip8->V[chip8->inst.X], chip8->keypad[chip8->inst.X]);
            }
            else if (chip8->inst.NN == 0xA1)
            {
                // 0xEX9E: Skip next instruciton if key in VX is not pressed
                 printf("Skip next instruction if key in V%X (0x%02X) is not pressed. Keypad value:  %d\n", chip8->inst.X, chip8->V[chip8->inst.X], chip8->keypad[chip8->inst.X]);
            }
            break;

        case 0x0F:
            switch (chip8->inst.NN)
            {
                case 0x0A:
                    // 0xFX0A : VX = get_key(); Await until a keypress, and store in VX
                    printf("Await until a key is pressed. Store key in V%X\n", chip8->inst.X);
                    break;

                case 0x1E:
                    // 0xFX1E: Set I += VX; Add VX to register I. 
                    printf("I (0x%04X) += V%X (0x%02X). Result (I): 0x%04X\n", chip8->I, chip8->inst.X, chip8->V[chip8->inst.X], chip8->I + chip8->V[chip8->inst.X]);
                    break;

                case 0x07:
                    // 0xFX07: Set VX = delay timer
                    printf("Set V%X = delay timer value (0x%02X)\n", chip8->V[chip8->inst.X], chip8->delay_timer);
                    break;

                case 0x15:
                    // 0xFX15: Set delay timer = VX
                    printf("Set delay timer value (0x%02X) = V%X\n", chip8->delay_timer, chip8->V[chip8->inst.X]);
                    break;

                case 0x18:
                    // 0xFX18: Set sound timer = VX
                    printf("Set sound timer value (0x%02X) = V%X\n", chip8->sound_timer, chip8->V[chip8->inst.X]);
                    break;

                case 0x29:
                    // 0xFX29: Set register I to sprite location in memory for character in VX (0x0 to 0xF)
                    printf("Set I to sprite location in memory for character in V%X (0x%02X). Result (VX*5) = (0x%02X)\n", chip8->inst.X, chip8->V[chip8->inst.X], chip8->V[chip8->inst.X] * 5);
                    break;

                case 0x33:
                    // 0xFX33: Store BCD representation of VX at memory offset from I
                    printf("Store BCD representation of V%X (0x%02X) at memory from I (0x%04X)\n", chip8->inst.X, chip8->V[chip8->inst.X], chip8->I);
                    break;

                case 0x55:
                    // 0xFX55: Register dump V0-VX inclusive to memory offset from I.
                    // SCHIP does not increment I, CHIP8 does increment I
                    printf("Register dump V0-V%X (0x%02X) inclusive at memory from I (0x%04X)\n", chip8->inst.X, chip8->V[chip8->inst.X], chip8->I);
                    break;

                case 0x65:
                    // 0xFX55: Register load V0-VX inclusive to memory offset from I.
                    // SCHIP does not increment I, CHIP8 does increment I
                    printf("Register load V0-V%X (0x%02X) inclusive at memory from I (0x%04X)\n", chip8->inst.X, chip8->V[chip8->inst.X], chip8->I);
                    break;

                default:    
                    // unimplemented or invalid opcode
                    break;
            }
            break;

        default:    
            printf("Unimplemented Opcode.\n");
            break;                        // unimplemented or invalid opcode
    }
}
#endif

// - - - - - - - - -
// VARS
// - - - - - - - - -

uint8_t WINDOW_WIDTH = 64;                // O.G. value
uint8_t WINDOW_HEIGHT = 32;               // O.G. value
uint32_t FG_COLOUR = 0xFFFFFFFF;          // RGBA8888, WHITE
uint32_t BG_COLOUR = 0x000000FF;          // RGBA8888, BLACK
int SCALE_FACTOR = 4;                     // 4* = 264 by 128 resolution
uint32_t INSTRUCTS_PER_SECOND = 700;      // CHIP8 CPU clock rate

// - - - - - - - - -
// HELPER METHODS
// - - - - - - - - - 

bool init_sdl(SDL_Window **window, SDL_Renderer **renderer)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0)
    {
        SDL_Log("Unable to initalize SDL: %s", SDL_GetError());
        return false;
    }

    *window = SDL_CreateWindow("CHIP8 EMULATOR", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH * SCALE_FACTOR, WINDOW_HEIGHT * SCALE_FACTOR, 0);
    if (!*window)
    {
        SDL_Log("Unable to initalize SDL window: %s", SDL_GetError());
        return false;
    }
    *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_ACCELERATED);
    if (!*renderer)
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
    const uint8_t r = (BG_COLOUR >> 24) & 0xFF;
    const uint8_t g = (BG_COLOUR >> 16) & 0xFF;
    const uint8_t b = (BG_COLOUR >>  8) & 0xFF;
    const uint8_t a = (BG_COLOUR >>  0) & 0xFF;

    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    SDL_RenderClear(renderer);
}

void update_screen(SDL_Renderer *renderer, const chip8_t chip8)
{
    SDL_Rect rect = { .x = 0, .y = 0, .w = SCALE_FACTOR, .h = SCALE_FACTOR };

    // grab colour values to draw (FG)
    const uint8_t fg_r = (FG_COLOUR >> 24) & 0xFF;
    const uint8_t fg_g = (FG_COLOUR >> 16) & 0xFF;
    const uint8_t fg_b = (FG_COLOUR >>  8) & 0xFF;
    const uint8_t fg_a = (FG_COLOUR >>  0) & 0xFF;

    // grab colour values to draw (BG)
    const uint8_t bg_r = (BG_COLOUR >> 24) & 0xFF;
    const uint8_t bg_g = (BG_COLOUR >> 16) & 0xFF;
    const uint8_t bg_b = (BG_COLOUR >>  8) & 0xFF;
    const uint8_t bg_a = (BG_COLOUR >>  0) & 0xFF;

    // loop through display pixels, draw a rectangle per pixel to the SDL window
    for (uint32_t i = 0; i < sizeof chip8.display; i++)
    {
        // translate 1d index i to 2d x,y coord
        rect.x = (i % WINDOW_WIDTH) * SCALE_FACTOR;
        rect.y = (i / WINDOW_WIDTH) * SCALE_FACTOR;

        if (chip8.display[i])
        {
            // pixel is on, draw FG colour
            SDL_SetRenderDrawColor(renderer, fg_r, fg_g, fg_b, fg_a);
            SDL_RenderFillRect(renderer, &rect);
        }
        else
        {
            // pixel is off, draw BG colour
            SDL_SetRenderDrawColor(renderer, bg_r, bg_g, bg_b, bg_a);
            SDL_RenderFillRect(renderer, &rect);
        }
    }

    // present the changes
    SDL_RenderPresent(renderer);
}

// - - - - - - - - -  - - - - - - - - -  - - - - - - - - - 
// INPUT MAP                                             |
// - - - - - - - - -  - - - - - - - - -  - - - - - - - - - 
// CHIP8 keyboard                        QWERTY          |
// ---                                   ---             |               
// 123C                                  1234            |
// 456D                                  qwer            |
// 789E                                  asdf            |
// A0BF                                  zxcv            |
// - - - - - - - - -  - - - - - - - - -  - - - - - - - - - 

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

                            puts("==== PAUSED ====");   // debug logging
                        }
                        else
                        {
                            chip8->state = RUNNING;    // Resume
                        }
                        return;

                    // map keyboard to chip8 keypad
                    case SDLK_1: chip8->keypad[0x1] = true; break;
                    case SDLK_2: chip8->keypad[0x2] = true; break;
                    case SDLK_3: chip8->keypad[0x3] = true; break;
                    case SDLK_4: chip8->keypad[0xC] = true; break;

                    case SDLK_q: chip8->keypad[0x4] = true; break;
                    case SDLK_w: chip8->keypad[0x5] = true; break;
                    case SDLK_e: chip8->keypad[0x6] = true; break;
                    case SDLK_r: chip8->keypad[0xD] = true; break;

                    case SDLK_a: chip8->keypad[0x7] = true; break;
                    case SDLK_s: chip8->keypad[0x8] = true; break;
                    case SDLK_d: chip8->keypad[0x9] = true; break;
                    case SDLK_f: chip8->keypad[0xE] = true; break;

                    case SDLK_z: chip8->keypad[0xA] = true; break;
                    case SDLK_x: chip8->keypad[0x0] = true; break;
                    case SDLK_c: chip8->keypad[0xB] = true; break;
                    case SDLK_v: chip8->keypad[0xF] = true; break;

                    default: break;
                }
                break;

            // key up event
            case SDL_KEYUP:

                switch (event.key.keysym.sym)
                {
                    // map keyboard to chip8 keypad
                    case SDLK_1: chip8->keypad[0x1] = false; break;
                    case SDLK_2: chip8->keypad[0x2] = false; break;
                    case SDLK_3: chip8->keypad[0x3] = false; break;
                    case SDLK_4: chip8->keypad[0xC] = false; break;

                    case SDLK_q: chip8->keypad[0x4] = false; break;
                    case SDLK_w: chip8->keypad[0x5] = false; break;
                    case SDLK_e: chip8->keypad[0x6] = false; break;
                    case SDLK_r: chip8->keypad[0xD] = false; break;

                    case SDLK_a: chip8->keypad[0x7] = false; break;
                    case SDLK_s: chip8->keypad[0x8] = false; break;
                    case SDLK_d: chip8->keypad[0x9] = false; break;
                    case SDLK_f: chip8->keypad[0xE] = false; break;

                    case SDLK_z: chip8->keypad[0xA] = false; break;
                    case SDLK_x: chip8->keypad[0x0] = false; break;
                    case SDLK_c: chip8->keypad[0xB] = false; break;
                    case SDLK_v: chip8->keypad[0xF] = false; break;

                    default: break;
                }
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
    const size_t max_size = sizeof chip8->ram - entry_point;
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

    // close the stream
    fclose(rom);

    // set chip8 machine defaults
    chip8->state = RUNNING;             // default machine state
    chip8->PC = entry_point;            // start program counter
    chip8->rom_name = rom_name;         // rom name
    chip8->stack_ptr = &chip8->stack[0];// stack ptr

    return true;
}

// emulate the CHIP-8 Instruction set
void emulate_instruction(chip8_t *chip8)
{
    // get next opcode from RAM
    chip8->inst.opcode = (chip8->ram[chip8->PC] << 8) | chip8->ram[chip8->PC+1];
    // pre-increment program counter for next opcode
    chip8->PC += 2;

    // fill out most recent instruction format
    chip8->inst.NNN = chip8->inst.opcode & 0x0FFF;
    chip8->inst.NN = chip8->inst.opcode & 0x0FF;
    chip8->inst.N = chip8->inst.opcode & 0x0F;
    chip8->inst.X = (chip8->inst.opcode >> 8) & 0x0F;
    chip8->inst.Y = (chip8->inst.opcode >> 4) & 0x0F;

#ifdef DEBUG
    print_debug_info(chip8);
#endif

    // emulate opcode
    switch ((chip8->inst.opcode >> 12) & 0x0F)
    {
        case 0x00:                       // if the top 12 bits are zero
            if (chip8->inst.NN == 0xE0)  // if the lowest 12 are
            {
                // 0x00E: Clear the screen
                // set the display memory to clear the screen
                memset(&chip8->display[0], false, sizeof chip8->display);
            }
            else if (chip8->inst.NN == 0xEE)
            {   
                // 0x00EE: Return from subroutine
                // Set program counter to last address on subroutine stack (pop it off the stack)
                // such that next opcode code will be taken from that address,
                chip8->PC = *--chip8->stack_ptr;
            }
            else
            {
                // unimplemented or invalid opcode
            } 
            break;

        case 0x01:
            // 0x1NNN: Jump to address NNN
            chip8->PC = chip8->inst.NNN;
            break;

        case 0x02:
            // 0x2NNN: Call subroutine at NNN
            // Store curent address for return to subroutine stack
            // and set program counter to subroutine address
            // such that the next opcode is taken from there.
            *chip8->stack_ptr++ = chip8->PC;
            chip8->PC = chip8->inst.NNN;
            break;

        case 0x03:
            // 0x3XNN: Check if VX == NN, if so, skip the next instruction
            if(chip8->V[chip8->inst.X] == chip8->inst.NN)
            {
                chip8->PC += 2;                     // skip next opcode
            }
            break;

        case 0x04:
            // 0x4XNN: Check if VX != NN, if so, skip the next instruction
            if(chip8->V[chip8->inst.X] != chip8->inst.NN)
            {
                chip8->PC += 2;                     // skip next opcode
            }
            break;

        case 0x05:
            // 0x5XY0: Check if VX == VY, if so, skip next instruction
            if (chip8->inst.N != 0) break;         // wrong opcode
            if(chip8->V[chip8->inst.X] == chip8->V[chip8->inst.Y])
            {
                chip8->PC += 2;                     // skip next opcode
            }
            break;
            
        case 0x06:
            // 0x6XNN: Set register VX to NN
            // V offset by X
            chip8->V[chip8->inst.X] = chip8->inst.NN;
            break;

        case 0x07:
            // 0x7XNN: Set register VX += NN
            chip8->V[chip8->inst.X] += chip8->inst.NN;
            break;

        case 0x08:
            switch (chip8->inst.N)
            {
                case 0:
                    // 0x8XY0: Set register VX = VY
                    chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y];
                    break;

                case 1:
                    // 0x8XY1: Set register VX |= VY
                    chip8->V[chip8->inst.X] |= chip8->V[chip8->inst.Y];
                    break;

                case 2:
                    // 0x8XY2: Set register VX &= VY
                    chip8->V[chip8->inst.X] &= chip8->V[chip8->inst.Y];
                    break;

                case 3:
                    // 0x8XY3: Set register VX ^= VY
                    chip8->V[chip8->inst.X] ^= chip8->V[chip8->inst.Y];
                    break;

                case 4:
                    // 0x8XY4: Set register VX += VY, set VF to 1 if carry, 0 if not
                    if ((uint16_t)(chip8->V[chip8->inst.X] + chip8->V[chip8->inst.Y]) > 255)
                    {
                        chip8->V[0xF] = 1;
                    }   
                    chip8->V[chip8->inst.X] += chip8->V[chip8->inst.Y];
                    break;

                case 5:
                    // 0x8XY5: Set register VX -= VY, set VF to 1 if there is not a borrow (result is positive)
                    chip8->V[0xF] = chip8->V[chip8->inst.Y] <= chip8->V[chip8->inst.X]; 
                    chip8->V[chip8->inst.X] -= chip8->V[chip8->inst.Y];
                    break;

                case 6:
                    // 0x8XY6: Set register VX >>= 1, store shifted off bit in VF
                    chip8->V[0xF] = chip8->V[chip8->inst.X] & 1;
                    chip8->V[chip8->inst.X] >>= 1;
                    break;

                case 7:
                    // 0x8XY7: Set register VX = VY - VX. set VF to 1 if there is not a borrow (result is positive)
                    chip8->V[0xF] = chip8->V[chip8->inst.X] <= chip8->V[chip8->inst.Y];
                    chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y] - chip8->V[chip8->inst.X];
                    break;

                case 0xE:
                    // 0x8XY8: Set register VX <<= 1, store shifted off bit in VF
                    chip8->V[0xF] = (chip8->V[chip8->inst.X] & 0x80) >> 7;
                    chip8->V[chip8->inst.X] <<= 1;
                    break;

                default:
                    // Wrong/Unimplemented opcode
                    break;
            }
            break;

        case 0x09:
            // 0x9XY0: Check if VX != VY; skip next instruction if so
            if (chip8->V[chip8->inst.X] != chip8->V[chip8->inst.Y])
            {
                chip8->PC += 2;
            }
            break;

        case 0x0A:
            // 0XANNN: Set index register I to NNN
            chip8->I = chip8->inst.NNN;
            break;

        case 0x0B:
            // 0xBNNN: Jump to V0 + NNN
            chip8->PC = chip8->V[0] + chip8->inst.NNN;
            break;

        case 0x0C:
            // 0xCXNN: Set register VX = rand() % 256 & NN (bitwise AND)
            chip8->V[chip8->inst.X] = (rand() % 256) & chip8->inst.NN;
            break;

        case 0x0D:
            // 0xDXYN: Draw N-height sprite at coordinate X,Y
            // Read from location memory I
            // Screen pixels are XOR'd with sprite bits, 
            // VF (Carry flag) is set if any screen pixels are set off; useful for collision detection

            // init vars
            uint8_t X = chip8->V[chip8->inst.X] % WINDOW_WIDTH;
            uint8_t Y = chip8->V[chip8->inst.Y] % WINDOW_HEIGHT;
            const uint8_t orig_X = X;

             // initalize carry flag to 0
            chip8->V[0xF] = 0;

            // loop over all N rows of the sprite
            for (uint8_t i = 0; i < chip8->inst.N; i++) 
            {
                // get next byte/row of sprite data
                const uint8_t sprite_data = chip8->ram[chip8->I + i];
                X = orig_X;                         // reset X for next row to draw

                // loop thru the row
                for (int j = 7; j >= 0; j--)
                {
                    // if sprite pixel/bit is on and display pixel is on, set carry flag
                    if (sprite_data & (1 << j) && chip8->display[Y * WINDOW_WIDTH + X])
                    {
                        chip8->V[0xF] = 1;          // set carry flag
                    }

                    // XOR display pixel with sprite pixel/bit to set it on or off
                    chip8->display[Y * WINDOW_WIDTH + X] ^= (sprite_data & (1 << j));

                    // Stop drawing if right edge of screen is hit
                    if (++X >= WINDOW_WIDTH) break;
                }
                
                // Stop drawing entire sprite if bottom edge of screen is hit
                if (++Y >= WINDOW_HEIGHT) break;
            }
            break;

        case 0x0E:
            if (chip8->inst.NN == 0x09E)
            {
                // 0xEX9E: Skip next instruction if key in VX is pressed
                if (chip8->keypad[chip8->V[chip8->inst.X]])
                {
                    chip8->PC += 2;
                }

            }
            else if (chip8->inst.NN == 0xA1)
            {
                // 0xEX9E: Skip next instruciton if key in VX is not pressed
                if (!chip8->keypad[chip8->V[chip8->inst.X]])
                {
                    chip8->PC += 2;
                }
            }
            break;
        
        case 0x0F:
            switch (chip8->inst.NN)
            {
                case 0x0A:
                    // 0xFX0A : VX = get_key(); Await until a keypress, and store in VX
                    bool any_key_pressed = false;
                    for (uint8_t i = 0; i < sizeof(chip8->keypad); i++)
                    {
                        if (chip8->keypad[i])
                        {   
                            chip8->V[chip8->inst.X] = i;                // i = key (offset into keypad array)
                            any_key_pressed = true;
                            break;
                        }
                    }

                    // If no key has been pressed, then keep grabbing the current opcode and run it.
                    if (!any_key_pressed) chip8->PC -= 2;
                    
                    break;

                case 0x1E:
                    // 0xFX1E: I += VX; Add VX to register I. 
                    chip8->I += chip8->V[chip8->inst.X];
                    break;

                case 0x07:
                    // 0xFX07: Set VX = delay timer
                    chip8->V[chip8->inst.X] = chip8->delay_timer;
                    break;

                case 0x15:
                    // 0xFX15: Set delay timer = VX
                    chip8->delay_timer = chip8->V[chip8->inst.X];
                    break;

                case 0x18:
                    // 0xFX15: Set sound timer = VX
                    chip8->sound_timer = chip8->V[chip8->inst.X];
                    break;

                case 0x29:
                    // 0xFX29: Set register I to sprite location in memory for character in VX (0x0 to 0xF)
                    chip8->I = chip8->V[chip8->inst.X] * 5;
                    break;

                case 0x33:
                    // 0xFX33: Store BCD representation of VX at memory offset from I
                    uint8_t bcd = chip8->V[chip8->inst.X];
                    chip8->ram[chip8->I+2] = bcd % 10;
                    bcd /= 10;
                    chip8->ram[chip8->I+1] = bcd % 10;
                    bcd /= 10;
                    chip8->ram[chip8->I] = bcd;
                    break;
                
                case 0x55:
                    // 0xFX55: Register dump V0-VX inclusive to memory offset from I;
                    // note: SCHIP does not increment I, CHIP8 does increment I
                    for (uint8_t i = 0; i <= chip8->inst.X; i++)
                    {
                        chip8->ram[chip8->I + i] = chip8->V[i];
                    }
                    break;

                case 0x65:
                    // 0xFX55: Register load V0-VX inclusive to memory offset from I.
                    // note: SCHIP does not increment I, CHIP8 does increment I
                    for (uint8_t i = 0; i <= chip8->inst.X; i++)
                    {
                       chip8->V[i] = chip8->ram[chip8->I + i];
                    }
                    break;

                default:    
                    // unimplemented or invalid opcode
                    break;
            }
            break;

        default:    
            // unimplemented or invalid opcode
            break;
    }
}

void update_timers(chip8_t *chip8)
{
    if (chip8->delay_timer > 0) chip8->delay_timer--;

    if (chip8->sound_timer > 0)
    {
        chip8->sound_timer--;

        // TODO: Play sound
    }
    else
    {
        // TODO: Stop playing sound
    }
}

// - - - - - - - - -
// MAIN PROGRAM
// - - - - - - - - - 

int main (int argc, char **argv)
{
    // arg handling
    // - - - - - - - -
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <Rom-Name>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    (void)argc;

    // initialization
    // - - - - - - - -
    SDL_Window *window = 0;
    SDL_Renderer *renderer = 0;

    // setup
    if (!init_sdl(&window, &renderer)) exit(EXIT_FAILURE);
    chip8_t chip8 = {0};
    if (!init_chip8(&chip8, argv[1])) exit(EXIT_FAILURE);

    // clear screen
    clear_screen(renderer);

    // seed the random num gen
    srand(time(NULL));

    // main emulator loop
    // - - - - - - - -
    while (chip8.state != QUIT)
    {
        // handle input
        handle_input(&chip8);

        if (chip8.state == PAUSED) continue;

        // get time before running instructions
        const uint64_t start = SDL_GetPerformanceCounter();

        // emulate CHIP8 insturctions for this emulator frame (60hz)
        for (uint32_t i = 0; i < INSTRUCTS_PER_SECOND / 60; i++)
        {
            emulate_instruction(&chip8);
        }
        // get time elapsed after running instructions
        const uint64_t end = SDL_GetPerformanceCounter();

        const double time_elapsed = (double)((end - start) * 1000) / SDL_GetPerformanceFrequency();

        // delay for approx. 60hz/60fps (16.67ms)
        SDL_Delay(16.67f > time_elapsed ? 16.67 - time_elapsed : 0);

        // update window with changes
        update_screen(renderer, chip8);

        // update delay and sound timers
        update_timers(&chip8);
    }

    // cleanup
    // - - - - - - - -
    final_sdl_cleanup(window, renderer);

    // exit the program
    exit(EXIT_SUCCESS);
}