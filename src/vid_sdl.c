// vid_sdl.h -- sdl video driver

#include "SDL.h"
#include "quakedef.h"
#include "d_local.h"

// viddef_t    vid;                // global video state - already defined in screen.c
unsigned short d_8to16table[256];

// The original defaults
//#define    BASEWIDTH    320
//#define    BASEHEIGHT   200
// Much better for high resolution displays
#define BASEWIDTH (320 * 2)
#define BASEHEIGHT (200 * 2)

int VGA_width, VGA_height, VGA_rowbytes, VGA_bufferrowbytes = 0;
byte* VGA_pagebase;

static SDL_Window* window = NULL;
static SDL_Surface* screen = NULL;

static qboolean mouse_avail;
static float mouse_x, mouse_y;
static int mouse_oldbuttonstate = 0;

// No support for option menus
// void (*vid_menudrawfn)(void);  // already defined in menu.c
// void (*vid_menukeyfn)(int key); // already defined in menu.c

void VID_SetPalette(unsigned char* palette)
{
    int i;
    SDL_Color colors[256];

    for (i = 0; i < 256; ++i) {
        colors[i].r = *palette++;
        colors[i].g = *palette++;
        colors[i].b = *palette++;
        colors[i].a = 255;
    }
    SDL_SetPaletteColors(screen->format->palette, colors, 0, 256);
}

void VID_ShiftPalette(unsigned char* palette)
{
    VID_SetPalette(palette);
}

void VID_Init(unsigned char* palette)
{
    int pnum, chunk;
    byte* cache;
    int cachesize;
    Uint32 flags;

    // Load the SDL library
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        Sys_Error("VID: Couldn't load SDL: %s", SDL_GetError());
    }

    // Set up display mode (width and height)
    vid.width = BASEWIDTH;
    vid.height = BASEHEIGHT;
    vid.maxwarpwidth = WARP_WIDTH;
    vid.maxwarpheight = WARP_HEIGHT;
    if ((pnum = COM_CheckParm("-winsize"))) {
        if (pnum >= com_argc - 2) {
            Sys_Error("VID: -winsize <width> <height>\n");
        }

        vid.width = Q_atoi(com_argv[pnum + 1]);
        vid.height = Q_atoi(com_argv[pnum + 2]);
        if (!vid.width || !vid.height) {
            Sys_Error("VID: Bad window width/height\n");
        }
    }

    // Set video width, height and flags
    flags = 0;
    if (COM_CheckParm("-fullscreen")) {
        flags |= SDL_WINDOW_FULLSCREEN;
    }

    // Create window
    window = SDL_CreateWindow("sdlquake",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        vid.width, vid.height, flags);

    if (!window) {
        Sys_Error("VID: Couldn't create window: %s\n", SDL_GetError());
    }

    // Get the window surface
    screen = SDL_GetWindowSurface(window);
    if (!screen) {
        Sys_Error("VID: Couldn't get window surface: %s\n", SDL_GetError());
    }

    // Check if we got an 8-bit surface (palette mode)
    if (screen->format->BitsPerPixel != 8) {
        // SDL2 doesn't always give us 8-bit surfaces directly
        // We need to create our own 8-bit surface
        SDL_Surface* new_screen = SDL_CreateRGBSurface(0, vid.width, vid.height, 8, 0, 0, 0, 0);
        if (!new_screen) {
            Sys_Error("VID: Couldn't create 8-bit surface: %s\n", SDL_GetError());
        }

        // Set up a palette for the 8-bit surface
        SDL_Palette* pal = SDL_AllocPalette(256);
        if (!pal) {
            Sys_Error("VID: Couldn't allocate palette: %s\n", SDL_GetError());
        }

        SDL_SetSurfaceBlendMode(new_screen, SDL_BLENDMODE_NONE);
        SDL_SetSurfacePalette(new_screen, pal);

        // Replace screen with our 8-bit surface
        screen = new_screen;
    }

    VID_SetPalette(palette);

    // now know everything we need to know about the buffer
    VGA_width = vid.conwidth = vid.width;
    VGA_height = vid.conheight = vid.height;
    vid.aspect = ((float)vid.height / (float)vid.width) * (320.0 / 240.0);
    vid.numpages = 1;
    vid.colormap = host_colormap;
    vid.fullbright = 256 - LittleLong(*((int*)vid.colormap + 2048));
    VGA_pagebase = vid.buffer = screen->pixels;
    VGA_rowbytes = vid.rowbytes = screen->pitch;
    vid.conbuffer = vid.buffer;
    vid.conrowbytes = vid.rowbytes;
    vid.direct = 0;

    // allocate z buffer and surface cache
    chunk = vid.width * vid.height * sizeof(*d_pzbuffer);
    cachesize = D_SurfaceCacheForRes(vid.width, vid.height);
    chunk += cachesize;
    d_pzbuffer = Hunk_HighAllocName(chunk, "video");
    if (d_pzbuffer == NULL) {
        Sys_Error("Not enough memory for video mode\n");
    }

    // initialize the cache memory
    cache = (byte*)d_pzbuffer + vid.width * vid.height * sizeof(*d_pzbuffer);
    D_InitCaches(cache, cachesize);

    // initialize the mouse
    SDL_ShowCursor(0);
}

void VID_Shutdown(void)
{
    if (window) {
        SDL_DestroyWindow(window);
        window = NULL;
    }

    SDL_Quit();
}

void VID_Update(vrect_t* rects)
{
    SDL_Rect* sdlrects;
    int n, i;
    vrect_t* rect;

    // Two-pass system, since Quake doesn't do it the SDL way...

    // First, count the number of rectangles
    n = 0;
    for (rect = rects; rect; rect = rect->pnext) {
        ++n;
    }

    // Second, copy them to SDL rectangles and update
    if (!(sdlrects = (SDL_Rect*)alloca(n * sizeof(*sdlrects)))) {
        Sys_Error("Out of memory");
    }

    i = 0;
    for (rect = rects; rect; rect = rect->pnext) {
        sdlrects[i].x = rect->x;
        sdlrects[i].y = rect->y;
        sdlrects[i].w = rect->width;
        sdlrects[i].h = rect->height;
        ++i;
    }

    // If we're using our own 8-bit surface, we need to blit it to the window
    SDL_Surface* window_surface = SDL_GetWindowSurface(window);
    if (screen != window_surface) {
        SDL_BlitSurface(screen, NULL, window_surface, NULL);
        SDL_UpdateWindowSurfaceRects(window, sdlrects, n);
    } else {
        SDL_UpdateWindowSurfaceRects(window, sdlrects, n);
    }
}

/*
================
D_BeginDirectRect
================
*/
void D_BeginDirectRect(int x, int y, byte* pbitmap, int width, int height)
{
    Uint8* offset;

    if (!screen) {
        return;
    }

    if (x < 0) {
        x = screen->w + x - 1;
    }

    offset = (Uint8*)screen->pixels + y * screen->pitch + x;
    while (height--) {
        memcpy(offset, pbitmap, width);
        offset += screen->pitch;
        pbitmap += width;
    }
}

/*
================
D_EndDirectRect
================
*/
void D_EndDirectRect(int x, int y, int width, int height)
{
    SDL_Rect rect;

    if (!screen || !window) {
        return;
    }

    if (x < 0) {
        x = screen->w + x - 1;
    }

    rect.x = x;
    rect.y = y;
    rect.w = width;
    rect.h = height;

    // If we're using our own 8-bit surface, we need to blit it to the window
    SDL_Surface* window_surface = SDL_GetWindowSurface(window);
    if (screen != window_surface) {
        SDL_BlitSurface(screen, &rect, window_surface, &rect);
    }

    SDL_UpdateWindowSurfaceRects(window, &rect, 1);
}

/*
================
Sys_SendKeyEvents
================
*/

void Sys_SendKeyEvents(void)
{
    SDL_Event event;
    int sym, state;
    int modstate;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            sym = event.key.keysym.sym;
            state = event.key.state;
            modstate = SDL_GetModState();
            switch (sym) {
            case SDLK_DELETE:
                sym = K_DEL;
                break;
            case SDLK_BACKSPACE:
                sym = K_BACKSPACE;
                break;
            case SDLK_F1:
                sym = K_F1;
                break;
            case SDLK_F2:
                sym = K_F2;
                break;
            case SDLK_F3:
                sym = K_F3;
                break;
            case SDLK_F4:
                sym = K_F4;
                break;
            case SDLK_F5:
                sym = K_F5;
                break;
            case SDLK_F6:
                sym = K_F6;
                break;
            case SDLK_F7:
                sym = K_F7;
                break;
            case SDLK_F8:
                sym = K_F8;
                break;
            case SDLK_F9:
                sym = K_F9;
                break;
            case SDLK_F10:
                sym = K_F10;
                break;
            case SDLK_F11:
                sym = K_F11;
                break;
            case SDLK_F12:
                sym = K_F12;
                break;
            case SDLK_PAUSE:
                sym = K_PAUSE;
                break;
            case SDLK_UP:
                sym = K_UPARROW;
                break;
            case SDLK_DOWN:
                sym = K_DOWNARROW;
                break;
            case SDLK_RIGHT:
                sym = K_RIGHTARROW;
                break;
            case SDLK_LEFT:
                sym = K_LEFTARROW;
                break;
            case SDLK_INSERT:
                sym = K_INS;
                break;
            case SDLK_HOME:
                sym = K_HOME;
                break;
            case SDLK_END:
                sym = K_END;
                break;
            case SDLK_PAGEUP:
                sym = K_PGUP;
                break;
            case SDLK_PAGEDOWN:
                sym = K_PGDN;
                break;
            case SDLK_RSHIFT:
            case SDLK_LSHIFT:
                sym = K_SHIFT;
                break;
            case SDLK_RCTRL:
            case SDLK_LCTRL:
                sym = K_CTRL;
                break;
            case SDLK_RALT:
            case SDLK_LALT:
                sym = K_ALT;
                break;
            case SDLK_KP_0:
                if (modstate & KMOD_NUM) {
                    sym = SDLK_0;
                } else {
                    sym = K_INS;
                }

                break;
            case SDLK_KP_1:
                if (modstate & KMOD_NUM) {
                    sym = SDLK_1;
                } else {
                    sym = K_END;
                }

                break;
            case SDLK_KP_2:
                if (modstate & KMOD_NUM) {
                    sym = SDLK_2;
                } else {
                    sym = K_DOWNARROW;
                }

                break;
            case SDLK_KP_3:
                if (modstate & KMOD_NUM) {
                    sym = SDLK_3;
                } else {
                    sym = K_PGDN;
                }

                break;
            case SDLK_KP_4:
                if (modstate & KMOD_NUM) {
                    sym = SDLK_4;
                } else {
                    sym = K_LEFTARROW;
                }

                break;
            case SDLK_KP_5:
                sym = SDLK_5;
                break;
            case SDLK_KP_6:
                if (modstate & KMOD_NUM) {
                    sym = SDLK_6;
                } else {
                    sym = K_RIGHTARROW;
                }

                break;
            case SDLK_KP_7:
                if (modstate & KMOD_NUM) {
                    sym = SDLK_7;
                } else {
                    sym = K_HOME;
                }

                break;
            case SDLK_KP_8:
                if (modstate & KMOD_NUM) {
                    sym = SDLK_8;
                } else {
                    sym = K_UPARROW;
                }

                break;
            case SDLK_KP_9:
                if (modstate & KMOD_NUM) {
                    sym = SDLK_9;
                } else {
                    sym = K_PGUP;
                }

                break;
            case SDLK_KP_PERIOD:
                if (modstate & KMOD_NUM) {
                    sym = SDLK_PERIOD;
                } else {
                    sym = K_DEL;
                }

                break;
            case SDLK_KP_DIVIDE:
                sym = SDLK_SLASH;
                break;
            case SDLK_KP_MULTIPLY:
                sym = SDLK_ASTERISK;
                break;
            case SDLK_KP_MINUS:
                sym = SDLK_MINUS;
                break;
            case SDLK_KP_PLUS:
                sym = SDLK_PLUS;
                break;
            case SDLK_KP_ENTER:
                sym = SDLK_RETURN;
                break;
            case SDLK_KP_EQUALS:
                sym = SDLK_EQUALS;
                break;
            }
            // If we're not directly handled and still above 255
            // just force it to 0
            if (sym > 255) {
                sym = 0;
            }

            Key_Event(sym, state);
            break;

        case SDL_MOUSEMOTION:
            if ((event.motion.x != (vid.width / 2)) || (event.motion.y != (vid.height / 2))) {
                mouse_x = event.motion.xrel * 10;
                mouse_y = event.motion.yrel * 10;
                if ((event.motion.x < ((vid.width / 2) - (vid.width / 4))) || (event.motion.x > ((vid.width / 2) + (vid.width / 4))) || (event.motion.y < ((vid.height / 2) - (vid.height / 4))) || (event.motion.y > ((vid.height / 2) + (vid.height / 4)))) {
                    SDL_WarpMouseInWindow(window, vid.width / 2, vid.height / 2);
                }
            }

            break;

        case SDL_QUIT:
            CL_Disconnect();
            Host_ShutdownServer(false);
            Sys_Quit();
            break;

        default:
            break;
        }
    }
}

void IN_Init(void)
{
    if (COM_CheckParm("-nomouse")) {
        return;
    }

    mouse_x = mouse_y = 0.0;
    mouse_avail = 1;
}

void IN_Shutdown(void)
{
    mouse_avail = 0;
}

void IN_Commands(void)
{
    int i;
    int mouse_buttonstate;

    if (!mouse_avail) {
        return;
    }

    i = SDL_GetMouseState(NULL, NULL);
    /* Quake swaps the second and third buttons */
    mouse_buttonstate = (i & ~0x06) | ((i & 0x02) << 1) | ((i & 0x04) >> 1);
    for (i = 0; i < 3; i++) {
        if ((mouse_buttonstate & (1 << i)) && !(mouse_oldbuttonstate & (1 << i))) {
            Key_Event(K_MOUSE1 + i, true);
        }

        if (!(mouse_buttonstate & (1 << i)) && (mouse_oldbuttonstate & (1 << i))) {
            Key_Event(K_MOUSE1 + i, false);
        }
    }
    mouse_oldbuttonstate = mouse_buttonstate;
}

void IN_Move(usercmd_t* cmd)
{
    if (!mouse_avail) {
        return;
    }

    mouse_x *= sensitivity.value;
    mouse_y *= sensitivity.value;

    if ((in_strafe.state & 1) || (lookstrafe.value && (in_mlook.state & 1))) {
        cmd->sidemove += m_side.value * mouse_x;
    } else {
        cl.viewangles[YAW] -= m_yaw.value * mouse_x;
    }

    if (in_mlook.state & 1) {
        V_StopPitchDrift();
    }

    if ((in_mlook.state & 1) && !(in_strafe.state & 1)) {
        cl.viewangles[PITCH] += m_pitch.value * mouse_y;
        if (cl.viewangles[PITCH] > 80) {
            cl.viewangles[PITCH] = 80;
        }

        if (cl.viewangles[PITCH] < -70) {
            cl.viewangles[PITCH] = -70;
        }
    } else {
        if ((in_strafe.state & 1) && noclip_anglehack) {
            cmd->upmove -= m_forward.value * mouse_y;
        } else {
            cmd->forwardmove -= m_forward.value * mouse_y;
        }
    }

    mouse_x = mouse_y = 0.0;
}

/*
================
Sys_ConsoleInput
================
*/
char* Sys_ConsoleInput(void)
{
    return 0;
}
