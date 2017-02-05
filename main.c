#include <stdint.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include <stdbool.h>

#define DISPLAY_HEIGHT 32
#define DISPLAY_WIDTH 64

const static uint8_t mem[0xFFF];

static uint8_t readByte(uint16_t loc) {
    if (loc < 0x200 || loc > 0xFFF) {
        fprintf(stderr, "Failed to read location 0x%03x\n", loc);
        return 0;
    }
    return mem[loc];
}

static uint16_t readBytes(uint16_t loc) {
    if (loc < 0x200 || loc > 0xFFF - sizeof(uint16_t)) {
        fprintf(stderr, "Failed to read location 0x%03x\n", loc);
        return 0;
    }
    return *((uint16_t *) &mem[loc]);
}

struct registers {
    uint8_t v0;
    uint8_t v1;
    uint8_t v2;
    uint8_t v3;
    uint8_t v4;
    uint8_t v5;
    uint8_t v6;
    uint8_t v7;
    uint8_t v8;
    uint8_t v9;
    uint8_t va;
    uint8_t vb;
    uint8_t vc;
    uint8_t vd;
    uint8_t ve;
    uint8_t vf;
    uint16_t i;

    uint16_t pc;
    uint8_t sp;
} registers;

static uint16_t stack[16];
static uint8_t stackLoc = 0;

static void stackPush(uint16_t val) {
    stackLoc--;
    if (stackLoc < 0) {
        stackLoc = 15;
    }
    stack[stackLoc] = val;
}

static uint16_t stackPeek() {
    return stack[stackLoc];
}

static uint16_t stackPop() {
    uint16_t val = stack[stackLoc];
    stackLoc = (uint8_t) ((stackLoc + 1) % 16);
    return val;
}

static uint8_t framebuffer[DISPLAY_WIDTH * DISPLAY_HEIGHT];

static void drawFramebuffer(SDL_Surface *surface) {
    for (int y = 0; y < DISPLAY_HEIGHT; ++y) {
        for (int x = 0; x < DISPLAY_WIDTH; ++x) {
            uint8_t c = (uint8_t) (framebuffer[x + (y * DISPLAY_WIDTH)] ? 0xFF : 0x00);
            SDL_Rect rect = {x * 4, y * 4, 4, 4};
            SDL_FillRect(surface, &rect, SDL_MapRGB(surface->format, c, c, c));
        }
    }
}

static bool executeInstruction(uint16_t op) {
    if (op == 0x00E0) {
        memset((void *) framebuffer, 0, sizeof(framebuffer));
    } else if (op == 0x00EE) {
        registers.pc = stack[stackLoc];
        registers.sp--;
    } else if ((op & 0xF000) == 0x1000) {
        registers.pc = (uint16_t) (op & 0x0FFF);
    } else if ((op & 0xF000) == 0x2000) {
        registers.sp++;
        stackPush(registers.pc);
    } else if ((op & 0xF000) == 0x300) {

    } else {
        fprintf(stderr, "Unknown instruction 0x%04x\n", op);
        return 1;
    }
    registers.pc++;
    return 0;
}

int main(int argc, char *argv[]) {
    FILE *ctt = fopen("CON", "w");
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        freopen("CON", "w", stderr);
        fprintf(stderr, "Failed to initialize SDL. Error: %s", SDL_GetError());
        return 1;
    }
    freopen("CON", "w", stdout);
    freopen("CON", "w", stderr);

    FILE *fh = fopen("games/TETRIS", "rb");
    if (fh == NULL) {
        fprintf(stderr, "Failed open ROM.");
        return 1;
    }
    fseek(fh, 0L, SEEK_END);
    size_t size = (size_t) ftell(fh);
    if (size < 1 || size > sizeof(mem) - 0x200) {
        fprintf(stderr, "Failed read ROM.");
        return 1;
    }
    rewind(fh);
    size_t read = fread((void *) &mem[0x200], sizeof(uint8_t), size, fh);
    if (read < size) {
        fprintf(stderr, "Failed read ROM.");
        return 1;
    }
    fclose(fh);

    SDL_Window *window = SDL_CreateWindow("CHIP-8", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                          DISPLAY_WIDTH * 4, DISPLAY_HEIGHT * 4, SDL_WINDOW_SHOWN);
    if (window == NULL) {
        fprintf(stderr, "Failed to initialize window. Error: %s", SDL_GetError());
        return 1;
    }
    SDL_Surface *surface = SDL_GetWindowSurface(window);
    drawFramebuffer(surface);
    SDL_UpdateWindowSurface(window);

    registers.pc = 0x200;
    while (!executeInstruction(registers.pc)) {
    }

    SDL_DestroyWindow(window);
    SDL_Quit();
    fclose(ctt);
    return 0;
}