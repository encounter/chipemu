#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <SDL/SDL.h>
#else
#include <SDL2/SDL.h>
#endif
#include <netinet/in.h>
#include <time.h>
#include <sys/time.h>
#include <sys/param.h>
#include <getopt.h>

#define DISPLAY_HEIGHT 32
#define DISPLAY_WIDTH 64
#define DISPLAY_SCALE 16
#define SPRITE_LOC 0x090
#define SPRITE_SIZE 0x50
#define STACK_LOC 0x0E0
#define STACK_ELEM_SIZE sizeof(uint16_t)
#define STACK_SIZE STACK_ELEM_SIZE * 16
#define VIDEO_LOC 0x100
#define VIDEO_SIZE (DISPLAY_WIDTH / 8) * DISPLAY_HEIGHT
#define ENTRY_POINT 0x200
#define MEM_SIZE 0xFFF

static uint8_t mem[MEM_SIZE];

static uint8_t readByte(uint16_t loc) {
    if ((loc < ENTRY_POINT || loc > MEM_SIZE)
        && !(loc >= SPRITE_LOC && loc <= SPRITE_LOC + SPRITE_SIZE)
        && !(loc >= STACK_LOC && loc <= STACK_LOC + STACK_SIZE)
        && !(loc >= VIDEO_LOC && loc <= VIDEO_LOC + VIDEO_SIZE)) {
        fprintf(stderr, "Failed to read location 0x%03x\n", loc);
        return (uint8_t) -1;
    }
    return mem[loc];
}

static uint16_t readBytes(uint16_t loc) {
    if ((loc < ENTRY_POINT || loc > MEM_SIZE - 1)
        && !(loc >= SPRITE_LOC && loc <= SPRITE_LOC + SPRITE_SIZE - 1)
        && !(loc >= STACK_LOC && loc <= STACK_LOC + STACK_SIZE - 1)
        && !(loc >= VIDEO_LOC && loc <= VIDEO_LOC + VIDEO_SIZE - 1)) {
        fprintf(stderr, "Failed to read location 0x%03x\n", loc);
        return (uint16_t) -1;
    }
    return htons(*((uint16_t *) &mem[loc]));
}

static bool writeByte(uint16_t loc, uint8_t val) {
    if ((loc < ENTRY_POINT || loc > MEM_SIZE - 1)
        && !(loc >= STACK_LOC && loc <= STACK_LOC + STACK_SIZE)
        && !(loc >= VIDEO_LOC && loc <= VIDEO_LOC + VIDEO_SIZE)) {
        fprintf(stderr, "Failed to write 0x%02x to location 0x%03x\n", val, loc);
        return false;
    }
    mem[loc] = val;
    return true;
}

static bool writeBytes(uint16_t loc, uint16_t val) {
    if ((loc < ENTRY_POINT || loc > MEM_SIZE - 1)
        && !(loc >= STACK_LOC && loc <= STACK_LOC + STACK_SIZE - 1)
        && !(loc >= VIDEO_LOC && loc <= VIDEO_LOC + VIDEO_SIZE - 1)) {
        fprintf(stderr, "Failed to write 0x%04x to location 0x%03x\n", val, loc);
        return false;
    }
    *((uint16_t *) &mem[loc]) = ntohs(val);
    return true;
}

struct {
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
    uint8_t dt;
    uint8_t st;
} registers;

void handleEvent(SDL_Event event);

static uint8_t *registerVx(uint8_t num) {
    if (num > 0xF) {
        fprintf(stderr, "Unknown register 0x%02x\n", num);
        return NULL;
    }
    return &registers.v0 + num;
}

static bool stackPush(uint16_t val) {
    registers.sp += STACK_ELEM_SIZE;
    if (registers.sp < STACK_LOC) {
        fprintf(stderr, "Stack overflow!");
        return false;
    }
    return writeBytes(registers.sp, val);
}

static uint16_t stackPop() {
    if (registers.sp == STACK_LOC) {
        fprintf(stderr, "Stack underrun!");
        return (uint16_t) -1;
    }
    uint16_t val = readBytes(registers.sp);
    registers.sp -= STACK_ELEM_SIZE;
    return val;
}

static const uint8_t sprites[SPRITE_SIZE] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
        0x20, 0x60, 0x20, 0x20, 0x70, // 1
        0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
        0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
        0x90, 0x90, 0xF0, 0x10, 0x10, // 4
        0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
        0xF0, 0x10, 0x20, 0x40, 0x40, // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90, // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
        0xF0, 0x80, 0x80, 0x80, 0xF0, // C
        0xE0, 0x90, 0x90, 0x90, 0xE0, // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
        0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

#ifdef __EMSCRIPTEN__
SDL_Surface *surface;

static void drawFramebuffer() {
#else
SDL_Window *window;

static void drawFramebuffer() {
    SDL_Surface *surface = SDL_GetWindowSurface(window);
#endif
    for (int i = 0; i < VIDEO_SIZE; ++i) {
        uint8_t val = readByte((uint16_t) (VIDEO_LOC + i));
        int x = i % (DISPLAY_WIDTH / 8);
        int y = i / (DISPLAY_WIDTH / 8);
        for (int v = 0; v < 8; ++v) {
            uint8_t c = (uint8_t) ((val & (0x80 >> v)) >> (7 - v) ? 0xFF : 0x00);
            struct SDL_Rect rect = {((x * 8) + v) * DISPLAY_SCALE, y * DISPLAY_SCALE,
                                    DISPLAY_SCALE, DISPLAY_SCALE};
            SDL_FillRect(surface, &rect, SDL_MapRGB(surface->format, c, c, c));
        }
    }
#ifndef __EMSCRIPTEN__
    SDL_UpdateWindowSurface(window);
#endif
}

static bool quit = false;
static bool paused = false;
static uint8_t *keyPressReg;
static bool keys[16];

// Thanks to mir3z/chip8-emu
struct {
    bool shift;
    bool loadStore;
} quirks;

static bool executeInstruction() {
    bool retVal = true;
    uint16_t op = readBytes(registers.pc);
    registers.pc += sizeof(registers.pc);

    if (op == 0x00E0) {
        // 00E0: CLS
        memset((void *) &mem[VIDEO_LOC], 0, VIDEO_SIZE);
        drawFramebuffer();
    } else if (op == 0x00EE) {
        // 00EE: RET
        registers.pc = stackPop();
        retVal = registers.pc != 1;
    } else if ((op & 0xF000) == 0x0000) {
        // 0xxx: NOP
    } else if ((op & 0xF000) == 0x1000) {
        // 1xxx: JP xxx
        uint16_t addr = (uint16_t) (op & 0x0FFF);
        if (registers.pc - sizeof(registers.pc) == addr) {
            // Detect infinite loop and pause execution
            paused = true;
        }
        registers.pc = addr;
    } else if ((op & 0xF000) == 0x2000) {
        // 2xxx: CALL xxx
        retVal = stackPush(registers.pc);
        registers.pc = (uint16_t) (op & 0x0FFF);
    } else if ((op & 0xF000) == 0x3000) {
        // 3xyy: SE Vx, yy
        if (*registerVx((uint8_t) ((op & 0x0F00) >> 8)) == (op & 0x00FF)) {
            registers.pc += sizeof(registers.pc);
        }
    } else if ((op & 0xF000) == 0x4000) {
        // 4xyy: SNE Vx, yy
        if (*registerVx((uint8_t) ((op & 0x0F00) >> 8)) != (op & 0x00FF)) {
            registers.pc += sizeof(registers.pc);
        }
    } else if ((op & 0xF00F) == 0x5000) {
        // 5xy0: SE Vx, Vy
        uint8_t vx = *registerVx((uint8_t) ((op & 0x0F00) >> 8));
        uint8_t vy = *registerVx((uint8_t) ((op & 0x00F0) >> 4));
        if (vx == vy) {
            registers.pc += sizeof(registers.pc);
        }
    } else if ((op & 0xF000) == 0x6000) {
        // 6xyy: LD Vx, yy
        *registerVx((uint8_t) ((op & 0x0F00) >> 8)) = (uint8_t) (op & 0x00FF);
    } else if ((op & 0xF000) == 0x7000) {
        // 7xyy: ADD Vx, yy
        *registerVx((uint8_t) ((op & 0x0F00) >> 8)) += (uint8_t) (op & 0x00FF);
    } else if ((op & 0xF00F) == 0x8000) {
        // 8xy0: LD Vx, Vy
        *registerVx((uint8_t) ((op & 0x0F00) >> 8)) =
                *registerVx((uint8_t) ((op & 0x00F0) >> 4));
    } else if ((op & 0xF00F) == 0x8001) {
        // 8xy1: OR Vx, Vy
        uint8_t *vx = registerVx((uint8_t) ((op & 0x0F00) >> 8));
        *vx = *vx | *registerVx((uint8_t) ((op & 0x00F0) >> 4));
    } else if ((op & 0xF00F) == 0x8002) {
        // 8xy2: AND Vx, Vy
        uint8_t *vx = registerVx((uint8_t) ((op & 0x0F00) >> 8));
        *vx = *vx & *registerVx((uint8_t) ((op & 0x00F0) >> 4));
    } else if ((op & 0xF00F) == 0x8003) {
        // 8xy3: XOR Vx, Vy
        uint8_t *vx = registerVx((uint8_t) ((op & 0x0F00) >> 8));
        *vx = *vx ^ *registerVx((uint8_t) ((op & 0x00F0) >> 4));
    } else if ((op & 0xF00F) == 0x8004) {
        // 8xy4: ADD Vx, Vy
        uint8_t *vx = registerVx((uint8_t) ((op & 0x0F00) >> 8));
        uint16_t val = ((uint16_t) *vx) + *registerVx((uint8_t) ((op & 0x00F0) >> 4));
        *vx = (uint8_t) val;
        registers.vf = (uint8_t) (val > 0xFF);
    } else if ((op & 0xF00F) == 0x8005) {
        // 8xy5: SUB Vx, Vy
        uint8_t *reg = registerVx((uint8_t) ((op & 0x0F00) >> 8));
        uint8_t vx = *reg;
        uint8_t vy = *registerVx((uint8_t) ((op & 0x00F0) >> 4));
        registers.vf = (uint8_t) (vx > vy);
        *reg = vx - vy;
    } else if ((op & 0xF00F) == 0x8006) {
        // 8xy6: SHR Vx, Vy
        uint8_t *regVx = registerVx((uint8_t) ((op & 0x0F00) >> 8));
        uint8_t *regVy = registerVx((uint8_t) ((op & 0x00F0) >> 4));
        if (quirks.shift) {
            regVy = regVx;
        }
        registers.vf = (uint8_t) (*regVy & 0x1);
        *regVx = *regVy >> 1;
    } else if ((op & 0xF00F) == 0x8007) {
        // 8xy7: SUBN Vx, Vy
        uint8_t *reg = registerVx((uint8_t) ((op & 0x0F00) >> 8));
        uint8_t vx = *reg;
        uint8_t vy = *registerVx((uint8_t) ((op & 0x00F0) >> 4));
        registers.vf = (uint8_t) (vy > vx);
        *reg = vy - vx;
    } else if ((op & 0xF00F) == 0x800E) {
        // 8xyE: SHL Vx {, Vy}
        uint8_t *regVx = registerVx((uint8_t) ((op & 0x0F00) >> 8));
        uint8_t *regVy = registerVx((uint8_t) ((op & 0x00F0) >> 4));
        if (quirks.shift) {
            regVy = regVx;
        }
        registers.vf = (uint8_t) ((*regVy >> 7) & 0x1);
        *regVx = *regVy << 1;
    } else if ((op & 0xF00F) == 0x9000) {
        // 9xy0: SNE Vx, Vy
        uint8_t vx = *registerVx((uint8_t) ((op & 0x0F00) >> 8));
        uint8_t vy = *registerVx((uint8_t) ((op & 0x00F0) >> 4));
        if (vx != vy) {
            registers.pc += sizeof(registers.pc);
        }
    } else if ((op & 0xF000) == 0xA000) {
        // Axxx: LD I, xxx
        registers.i = (uint16_t) (op & 0x0FFF);
    } else if ((op & 0xF000) == 0xB000) {
        // Bxxx: JP V0, xxx
        registers.pc = (uint16_t) (registers.v0 + (op & 0x0FFF));
    } else if ((op & 0xF000) == 0xC000) {
        // Cxyy: RND Vx, yy
        uint8_t val = (uint8_t) ((rand() % 0xFF) & (op & 0x00FF));
        *registerVx((uint8_t) ((op & 0x0F00) >> 8)) = val;
    } else if ((op & 0xF000) == 0xD000) {
        // Dxyn: DRW Vx, Vy, nibble
        uint8_t x = *registerVx((uint8_t) ((op & 0x0F00) >> 8));
        uint8_t y = *registerVx((uint8_t) ((op & 0x00F0) >> 4));
        registers.vf = 0;
        for (int i = 0; i < (op & 0x000F); ++i) {
            uint16_t val = readByte((uint16_t) (registers.i + i)) << (8 - (x % 8));
            uint16_t loc = (uint16_t) (VIDEO_LOC + ((x % DISPLAY_WIDTH) / 8)
                                       + (((y + i) % DISPLAY_HEIGHT) * (DISPLAY_WIDTH / 8)));
            uint16_t bytes = readBytes(loc);
            writeBytes(loc, bytes ^ val);
            registers.vf = (uint8_t) (registers.vf || ((bytes & val) ? 1 : 0));
        }
        drawFramebuffer();
    } else if ((op & 0xF0FF) == 0xE09E) {
        // Ex9E: SKP Vx
        if (keys[*registerVx((uint8_t) ((op & 0x0F00) >> 8))])
            registers.pc += sizeof(registers.pc);
    } else if ((op & 0xF0FF) == 0xE0A1) {
        // ExA1: SKNP Vx
        if (!keys[*registerVx((uint8_t) ((op & 0x0F00) >> 8))])
            registers.pc += sizeof(registers.pc);
    } else if ((op & 0xF0FF) == 0xF007) {
        // Fx07: LD Vx, DT
        *registerVx((uint8_t) ((op & 0x0F00) >> 8)) = registers.dt;
    } else if ((op & 0xF0FF) == 0xF00A) {
        // Fx0A: LD Vx, K
        keyPressReg = registerVx((uint8_t) ((op & 0x0F00) >> 8));
        paused = true;
    } else if ((op & 0xF0FF) == 0xF015) {
        // Fx15: LD DT, Vx
        registers.dt = *registerVx((uint8_t) ((op & 0x0F00) >> 8));
    } else if ((op & 0xF0FF) == 0xF018) {
        // Fx18: LD ST, Vx
        // TODO implement sound timer
    } else if ((op & 0xF0FF) == 0xF01E) {
        // Fx1E: ADD I, Vx
        registers.i += *registerVx((uint8_t) ((op & 0x0F00) >> 8));
    } else if ((op & 0xF0FF) == 0xF029) {
        // Fx29: LD I, sprite for Vx
        uint8_t num = *registerVx((uint8_t) ((op & 0x0F00) >> 8));
        registers.i = (uint16_t) (SPRITE_LOC + (num * 5));
    } else if ((op & 0xF0FF) == 0xF033) {
        // Fx33: LD B, Vx
        // TODO optimize?
        uint8_t num = *registerVx((uint8_t) ((op & 0x0F00) >> 8));
        uint8_t hundreds = (uint8_t) (num / 100);
        writeByte(registers.i, hundreds);
        uint8_t tens = (uint8_t) ((num - (hundreds * 100)) / 10);
        writeByte((uint16_t) (registers.i + 1), tens);
        uint8_t ones = (uint8_t) (num - (hundreds * 100) - (tens * 10));
        writeByte((uint16_t) (registers.i + 2), ones);
    } else if ((op & 0xF0FF) == 0xF055) {
        // Fx55: LD [I], Vx
        uint8_t end = (uint8_t) ((op & 0x0F00) >> 8);
        for (uint8_t i = 0; i <= end; ++i) {
            writeByte(registers.i + i, *registerVx(i));
        }
        if (!quirks.loadStore) {
            registers.i += end + 1;
        }
    } else if ((op & 0xF0FF) == 0xF065) {
        // Fx65: LD Vx, [I]
        uint8_t end = (uint8_t) ((op & 0x0F00) >> 8);
        for (uint8_t i = 0; i <= end; ++i) {
            *registerVx(i) = readByte(registers.i + i);
        }
        if (!quirks.loadStore) {
            registers.i += end + 1;
        }
    } else {
        fprintf(stderr, "Unknown instruction 0x%04x at 0x%04x\n", op, registers.pc);
        retVal = false;
    }
    nanosleep((const struct timespec[]) {{0, 1000000L}}, NULL);
    return retVal;
}

double ms, interval = 1000.0 / 60;
struct timeval t1, t2;
SDL_Event event;

static void reset() {
    gettimeofday(&t1, NULL);

    memset((void *) &mem[VIDEO_LOC], 0, VIDEO_SIZE);
    memcpy((void *) &mem[SPRITE_LOC], sprites, sizeof(sprites));
    drawFramebuffer();
    srand((uint16_t) time(NULL));

    registers.sp = STACK_LOC;
    registers.pc = ENTRY_POINT;
}

#ifdef __EMSCRIPTEN__
void mainLoop() {
    if (quit) {
        emscripten_cancel_main_loop();
        return;
    }
#else
bool mainLoop() {
#endif
    while (SDL_PollEvent(&event)) {
        handleEvent(event);
    }
    if (paused) {
#ifdef __EMSCRIPTEN__
        return;
#else
        if (!quit)
            SDL_WaitEvent(NULL);
        return true;
#endif
    }
#ifdef __EMSCRIPTEN__
    int count = 0;
    while (count++ < 5) {
        if (!executeInstruction()) {
            emscripten_cancel_main_loop();
            return;
        }
    }
#else
    if (!executeInstruction()) {
        return false;
    }
#endif
    gettimeofday(&t2, NULL);
    ms = (t2.tv_sec - t1.tv_sec) * 1000.0;
    ms += (t2.tv_usec - t1.tv_usec) / 1000.0;
    if (ms > interval) {
        registers.dt = (uint8_t) MAX(registers.dt - (ms / interval), 0);
        registers.st = (uint8_t) MAX(registers.st - (ms / interval), 0);
        gettimeofday(&t1, NULL);
    }
#ifndef __EMSCRIPTEN__
    return true;
#endif
}

int main(int argc, char *argv[]) {
#ifdef __EMSCRIPTEN__
    char *filename = "games/TETRIS";
#else
    char *filename = NULL;
    struct option long_opts[] = {
            {"quirk-shift", no_argument, (int *) &quirks.shift, true},
            {"quirk-loadstore", no_argument, (int *) &quirks.loadStore, true}
    };
    int optIndex;
    while ((getopt_long(argc, argv, "", long_opts, &optIndex)) != -1) {
    }
    // TODO improve parsing
    for (int i = 1; i < argc; ++i) {
        char *arg = argv[i];
        if (!strcmp(arg, "--")) {
            break;
        } else if (arg[0] != '-') {
            filename = arg;
            break;
        }
    }
    if (filename == NULL) {
        fprintf(stderr, "Usage: chipemu [rom] [--quirk-shift] [--quirk-loadstore]\n");
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "Failed to initialize SDL. Error: %s\n", SDL_GetError());
        return 1;
    }
#endif

    FILE *fh = fopen(filename, "rb");
    if (fh == NULL) {
        fprintf(stderr, "Failed open ROM %s\n", filename);
        return 1;
    }
    fseek(fh, 0L, SEEK_END);
    size_t size = (size_t) ftell(fh);
    if (size < 1 || size > sizeof(mem) - ENTRY_POINT) {
        fprintf(stderr, "Failed read ROM.\n");
        return 1;
    }
    rewind(fh);
    size_t read = fread((void *) &mem[ENTRY_POINT], sizeof(uint8_t), size, fh);
    if (read < size) {
        fprintf(stderr, "Failed read ROM.\n");
        return 1;
    }
    fclose(fh);

#ifdef __EMSCRIPTEN__
    surface = SDL_SetVideoMode(DISPLAY_WIDTH * DISPLAY_SCALE, DISPLAY_HEIGHT * DISPLAY_SCALE,
                               32, SDL_SWSURFACE);
    if (surface == NULL) {
        fprintf(stderr, "Failed to initialize surface. Error: %s\n", SDL_GetError());
        return 1;
    }
#else
    window = SDL_CreateWindow("CHIP-8", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                              DISPLAY_WIDTH * DISPLAY_SCALE, DISPLAY_HEIGHT * DISPLAY_SCALE,
                              SDL_WINDOW_SHOWN);
    if (window == NULL) {
        fprintf(stderr, "Failed to initialize window. Error: %s\n", SDL_GetError());
        return 1;
    }
#endif

    printf("Starting %s...\n", filename);
    reset();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(mainLoop, 0, 1);
#else
    while (!quit && !mainLoop()) {
    }
#endif

#ifndef __EMSCRIPTEN__
    SDL_DestroyWindow(window);
#endif
    SDL_Quit();
    return 0;
}

static uint32_t mappings[16] = {
        SDLK_x, SDLK_1, SDLK_2, SDLK_3,
        SDLK_q, SDLK_w, SDLK_e, SDLK_a,
        SDLK_s, SDLK_d, SDLK_z, SDLK_c,
        SDLK_4, SDLK_r, SDLK_f, SDLK_v
};

void handleEvent(SDL_Event event) {
    switch (event.type) {
        case SDL_KEYDOWN:
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                reset();
                break;
            }
            for (uint8_t k = 0; k < sizeof(mappings); ++k) {
                if (event.key.keysym.sym == mappings[k]) {
                    keys[k] = true;
                    if (paused && keyPressReg) {
                        *keyPressReg = k;
                        keyPressReg = NULL;
                        paused = false;
                    }
                    break;
                }
            }
            break;
        case SDL_KEYUP:
            for (uint8_t k = 0; k < sizeof(mappings); ++k) {
                if (event.key.keysym.sym == mappings[k]) {
                    keys[k] = false;
                    break;
                }
            }
            break;
        case SDL_QUIT:
            quit = true;
            break;
        default:
            break;
    }
}
