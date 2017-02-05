#include <stdint.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <time.h>
#include <sys/time.h>
#include <sys/param.h>

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
        && !(loc >= SPRITE_LOC && loc <= SPRITE_LOC + SPRITE_SIZE)
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
        && !(loc >= SPRITE_LOC && loc <= SPRITE_LOC + SPRITE_SIZE - 1)
        && !(loc >= STACK_LOC && loc <= STACK_LOC + STACK_SIZE - 1)
        && !(loc >= VIDEO_LOC && loc <= VIDEO_LOC + VIDEO_SIZE - 1)) {
        fprintf(stderr, "Failed to write 0x%04x to location 0x%03x\n", val, loc);
        return false;
    }
    *((uint16_t *) &mem[loc]) = ntohs(val);
    return true;
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
    uint8_t dt;
    uint8_t st;
} registers;

void handleEvent(SDL_Event event);

static uint8_t *registerVx(uint8_t num) {
    switch (num) {
        case 0x0:
            return &registers.v0;
        case 0x1:
            return &registers.v1;
        case 0x2:
            return &registers.v2;
        case 0x3:
            return &registers.v3;
        case 0x4:
            return &registers.v4;
        case 0x5:
            return &registers.v5;
        case 0x6:
            return &registers.v6;
        case 0x7:
            return &registers.v7;
        case 0x8:
            return &registers.v8;
        case 0x9:
            return &registers.v9;
        case 0xA:
            return &registers.va;
        case 0xB:
            return &registers.vb;
        case 0xC:
            return &registers.vc;
        case 0xD:
            return &registers.vd;
        case 0xE:
            return &registers.ve;
        case 0xF:
            return &registers.vf;
        default:
            fprintf(stderr, "Unknown register 0x%02x\n", num);
            return NULL;
    }
}

static bool stackPush(uint16_t val) {
    registers.sp += STACK_ELEM_SIZE;
    if (registers.sp > STACK_LOC + STACK_SIZE) {
        fprintf(stderr, "Stack overflow!");
        return false;
    }
    printf("stack push 0x%04x\n", val);
    return writeBytes(registers.sp, val);
}

static uint16_t stackPeek() {
    return readBytes(registers.sp);
}

static uint16_t stackPop() {
    if (registers.sp == STACK_LOC) {
        fprintf(stderr, "Stack underrun!");
        return (uint16_t) -1;
    }
    uint16_t val = readBytes(registers.sp);
    printf("stack pop 0x%04x\n", val);
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

SDL_Window *window;

static void drawFramebuffer(SDL_Window *window) {
    SDL_Surface *surface = SDL_GetWindowSurface(window);
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
    SDL_UpdateWindowSurface(window);
}

static bool quit = false;
static bool pause = false;
static uint8_t *keyPressReg;
static bool keys[16];

static bool executeInstruction() {
    bool retVal = true;
    uint16_t op = readBytes(registers.pc);
    printf("-- 0x%04x: exec 0x%04x\n", registers.pc, op);
    registers.pc += sizeof(registers.pc);

    if (op == 0x00E0) {
        // 00E0: CLS
        memset((void *) &mem[VIDEO_LOC], 0, VIDEO_SIZE);
        drawFramebuffer(window);
    } else if (op == 0x00EE) {
        // 00EE: RET
        uint16_t val = stackPop();
        registers.pc = val;
        printf("jmp to 0x%04x\n", registers.pc);
        retVal = val != 1;
    } else if ((op & 0xF000) == 0x0000) {
        // 0xxx: NOP
    } else if ((op & 0xF000) == 0x1000) {
        // 1xxx: JP xxx
        uint16_t addr = (uint16_t) (op & 0x0FFF);
        if (registers.pc - sizeof(registers.pc) == addr) {
            // Detect infinite loop and pause execution
            pause = true;
        }
        registers.pc = addr;
        printf("jmp to 0x%04x\n", registers.pc);
    } else if ((op & 0xF000) == 0x2000) {
        // 2xxx: CALL xxx
        retVal = stackPush(registers.pc);
        registers.pc = (uint16_t) (op & 0x0FFF);
        printf("jmp to 0x%04x\n", registers.pc);
    } else if ((op & 0xF000) == 0x3000) {
        // 3xyy: SE Vx, yy
        uint8_t reg = (uint8_t) ((op & 0x0F00) >> 8);
        uint8_t val = *registerVx(reg);
        printf("checking reg v%02x, val 0x%02x == 0x%02x\n", reg, val, op & 0x00FF);
        if (val == (op & 0x00FF)) {
            printf("skipping next instruction\n");
            registers.pc += sizeof(registers.pc);
        }
    } else if ((op & 0xF000) == 0x4000) {
        // 4xyy: SNE Vx, yy
        uint8_t reg = (uint8_t) ((op & 0x0F00) >> 8);
        uint8_t val = *registerVx(reg);
        printf("checking reg v%02x, val 0x%02x != 0x%02x\n", reg, val, op & 0x00FF);
        if (val != (op & 0x00FF)) {
            printf("skipping next instruction\n");
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
        uint8_t reg = (uint8_t) ((op & 0x0F00) >> 8);
        printf("setting reg v%02x = 0x%04x\n", reg, op & 0x00FF);
        *registerVx(reg) = (uint8_t) (op & 0x00FF);
    } else if ((op & 0xF000) == 0x7000) {
        // 7xyy: ADD Vx, yy
        uint8_t reg = (uint8_t) ((op & 0x0F00) >> 8);
        printf("adding reg v%02x += 0x%04x\n", reg, op & 0x00FF);
        *registerVx(reg) += (uint8_t) (op & 0x00FF);
        printf("reg v%02x = 0x%04x\n", reg, *registerVx(reg));
    } else if ((op & 0xF00F) == 0x8000) {
        // 8xy0: LD Vx, Vy
        *registerVx((uint8_t) ((op & 0x0F00) >> 8)) =
                *registerVx((uint8_t) ((op & 0x00F0) >> 4));
    } else if ((op & 0xF00F) == 0x8001) {
        // 8xy2: OR Vx, Vy
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
        // 8xy6: SHR Vx {, Vy}
        uint8_t *reg = registerVx((uint8_t) ((op & 0x0F00) >> 8));
        uint8_t vx = *reg;
        registers.vf = (uint8_t) (vx & 0x1);
        *reg = (uint8_t) (vx / 2);
        // TODO figure out if this is right.......
    } else if ((op & 0xF00F) == 0x8007) {
        // 8xy7: SUBN Vx, Vy
        uint8_t *reg = registerVx((uint8_t) ((op & 0x0F00) >> 8));
        uint8_t vx = *reg;
        uint8_t vy = *registerVx((uint8_t) ((op & 0x00F0) >> 4));
        registers.vf = (uint8_t) (vy > vx);
        *reg = vy - vx;
    } else if ((op & 0xF00F) == 0x800E) {
        // 8xyE: SHL Vx {, Vy}
        uint8_t *reg = registerVx((uint8_t) ((op & 0x0F00) >> 8));
        uint8_t vx = *reg;
        registers.vf = (uint8_t) (vx & 0x100) >> 8;
        *reg = (uint8_t) (vx * 2);
        // TODO figure out if this is right.......
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
        printf("jmp to 0x%04x (v0 0x%04x + 0x%04x)\n", registers.pc, registers.v0, (op & 0x0FFF));
    } else if ((op & 0xF000) == 0xC000) {
        // Cxyy: RND Vx, yy
        uint8_t reg = (uint8_t) ((op & 0x0F00) >> 8);
        uint8_t val = (uint8_t) ((rand() % 0xFF) & (op & 0x00FF));
        printf("random reg v%02x = 0x%02x\n", reg, val);
        *registerVx(reg) = val;
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
        drawFramebuffer(window);
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
        uint8_t reg = (uint8_t) ((op & 0x0F00) >> 8);
        *registerVx(reg) = registers.dt;
        printf("loading dt %02x into reg v%02x\n", registers.dt, reg);
    } else if ((op & 0xF0FF) == 0xF00A) {
        // Fx0A: LD Vx, K
        keyPressReg = registerVx((uint8_t) ((op & 0x0F00) >> 8));
        pause = true;
    } else if ((op & 0xF0FF) == 0xF015) {
        // Fx15: LD DT, Vx
        uint8_t reg = (uint8_t) ((op & 0x0F00) >> 8);
        registers.dt = *registerVx(reg);
        printf("loading reg v%02x into dt %02x\n", reg, registers.dt);
    } else if ((op & 0xF0FF) == 0xF018) {
        // Fx18: LD ST, Vx
        // TODO implement sound timer
    } else if ((op & 0xF0FF) == 0xF01E) {
        // Fx1E: ADD I, Vx
        uint8_t reg = (uint8_t) ((op & 0x0F00) >> 8);
        uint8_t regVal = *registerVx(reg);
        printf("adding I 0x%04x += reg v%02x 0x%02x\n", registers.i, reg, regVal);
        registers.i += regVal;
        printf("reg I = 0x%04x\n", registers.i);
    } else if ((op & 0xF0FF) == 0xF029) {
        // Fx29: LD I, sprite for Vx
        uint8_t num = *registerVx((uint8_t) ((op & 0x0F00) >> 8));
        registers.i = (uint16_t) (SPRITE_LOC + (num * 5));
        printf("read sprite location for %01x into I 0x%04x\n", num, registers.i);
    } else if ((op & 0xF0FF) == 0xF033) {
        // Fx33: LD B, Vx
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
            uint8_t val = *registerVx(i);
            printf("storing reg v%02x 0x%02x at 0x%04x\n", i, val, registers.i + i);
            writeByte(registers.i + i, val);
        }
    } else if ((op & 0xF0FF) == 0xF065) {
        uint8_t end = (uint8_t) ((op & 0x0F00) >> 8);
        for (uint8_t i = 0; i <= end; ++i) {
            uint8_t val = readByte(registers.i + i);
            printf("loading 0x%02x at 0x%04x into reg v%02x\n", val, registers.i + i, i);
            *registerVx(i) = val;
        }
    } else {
        fprintf(stderr, "Unknown instruction 0x%04x at 0x%04x\n", op, registers.pc);
        retVal = false;
    }
    nanosleep((const struct timespec[]) {{0, 1000000L}}, NULL);
    return retVal;
}

int main(int argc, char *argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "Failed to initialize SDL. Error: %s\n", SDL_GetError());
        return 1;
    }

    char *filename = "games/TETRIS";
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

    /*SDL_Window **/window = SDL_CreateWindow("CHIP-8", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                              DISPLAY_WIDTH * DISPLAY_SCALE, DISPLAY_HEIGHT * DISPLAY_SCALE,
                                              SDL_WINDOW_SHOWN);
    if (window == NULL) {
        fprintf(stderr, "Failed to initialize window. Error: %s\n", SDL_GetError());
        return 1;
    }

    memcpy((void *) &mem[SPRITE_LOC], sprites, sizeof(sprites));
    drawFramebuffer(window);

    //setbuf(stdout, NULL);
    //setbuf(stderr, NULL);
    srand((uint16_t) time(NULL));

    double ms, interval = 1000.0 / 60;
    struct timeval t1, t2;
    gettimeofday(&t1, NULL);

    registers.sp = STACK_LOC;
    registers.pc = ENTRY_POINT;

    SDL_Event event;
    while (!quit) {
        while (SDL_PollEvent(&event)) {
            handleEvent(event);
        }
        if (pause) {
            if (!quit)
                SDL_WaitEvent(NULL);
            continue;
        }
        if (!executeInstruction()) {
            break;
        }
        gettimeofday(&t2, NULL);
        ms = (t2.tv_sec - t1.tv_sec) * 1000.0;
        ms += (t2.tv_usec - t1.tv_usec) / 1000.0;
        if (ms > interval) {
            registers.dt = (uint8_t) MAX(registers.dt - (ms / interval), 0);
            registers.st = (uint8_t) MAX(registers.st - (ms / interval), 0);
            gettimeofday(&t1, NULL);
        }
    }

    SDL_DestroyWindow(window);
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
            for (uint8_t k = 0; k < sizeof(mappings); ++k) {
                if (event.key.keysym.sym == mappings[k]) {
                    keys[k] = true;
                    if (pause && keyPressReg) {
                        *keyPressReg = k;
                        keyPressReg = NULL;
                        pause = false;
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