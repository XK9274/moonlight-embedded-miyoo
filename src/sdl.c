/*
 * This file is part of Moonlight Embedded.
 *
 * Copyright (C) 2015 Iwan Timmer
 *
 * Moonlight is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Moonlight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Moonlight; if not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_SDL

#include "sdl.h"
#include "input/sdl.h"
#include <Limelight.h>
#include "util.h"

static bool done;
static int fullscreen_flags;
extern int unPair;
extern int pairNow;
SDL_mutex *mutex;

int sdlCurrentFrame, sdlNextFrame;

void sdl_base_ui(SDLContext *ctx) {
    SDL_FillRect(ctx->menu_surface, NULL, SDL_MapRGB(ctx->menu_surface->format, 40, 39, 46));

    SDL_Surface* png_surface = IMG_Load(TOP_BANNER);
    if (!png_surface) {
        fprintf(stderr, "Could not load PNG image: %s\n", IMG_GetError());
    } else {
        int x_position = (ctx->menu_surface->w - png_surface->w) / 2;

        SDL_Rect dest_rect;
        dest_rect.x = x_position;
        dest_rect.y = 0;
        dest_rect.w = png_surface->w;
        dest_rect.h = png_surface->h;

        SDL_BlitSurface(png_surface, NULL, ctx->menu_surface, &dest_rect);
        SDL_FreeSurface(png_surface);
    }
}

void sdl_init(SDLContext *ctx, int width, int height, bool fullscreen) {
    ctx->sdlCurrentFrame = ctx->sdlNextFrame = 0;

    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
        exit(1);
    }

    Uint32 fullscreen_flags = fullscreen ? SDL_WINDOW_FULLSCREEN : 0;
    ctx->window = SDL_CreateWindow("Moonlight", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_OPENGL | fullscreen_flags);

    if(!ctx->window) {
        fprintf(stderr, "SDL: could not create window - exiting\n");
        exit(1);
    }

    ctx->renderer = SDL_CreateRenderer(ctx->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ctx->renderer) {
    printf("SDL_CreateRenderer failed: %s\n", SDL_GetError());
    exit(1);
    }

    ctx->bmp = SDL_CreateTexture(ctx->renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!ctx->bmp) {
    fprintf(stderr, "SDL: could not create texture - exiting\n");
    exit(1);
    }

    mutex = SDL_CreateMutex();
    if (!mutex) {
    fprintf(stderr, "Couldn't create mutex\n");
    exit(1);
    }

    SDL_RendererInfo info;
    if (SDL_GetRendererInfo(ctx->renderer, &info) == 0) {
        printf("Renderer Name: %s\n", info.name);
        for (Uint32 i = 0; i < info.num_texture_formats; i++) {
            printf("Texture Format %d: %s\n", i, SDL_GetPixelFormatName(info.texture_formats[i]));
        }
    }
    
    ctx->menu_surface = SDL_CreateRGBSurface(0, 640, 480, 16, 0xF800, 0x07E0, 0x001F, 0x0000);
    ctx->menu_texture = SDL_CreateTexture(ctx->renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, 640, 480);
    sdl_base_ui(ctx);
        
    if(TTF_Init()) {
        fprintf(stderr, "Could not initialize TTF - %s\n", SDL_GetError());
        exit(1);
    }
}


void sdl_banner(SDLContext *ctx, const char *text, const char *color) {
    if (!ctx || !ctx->menu_surface) return;

    SDL_Rect rect;
    rect.x = 0;
    rect.y = 0;
    rect.w = 640;
    rect.h = 40;

    Uint32 bannerColor;

    if (strcmp(color, "green") == 0) {
        bannerColor = SDL_MapRGB(ctx->menu_surface->format, 0, 255, 0);
    } else {
        bannerColor = SDL_MapRGB(ctx->menu_surface->format, 255, 0, 0);
    }

    SDL_FillRect(ctx->menu_surface, &rect, bannerColor);


    if (text) {
        TTF_Font *font = TTF_OpenFont(MOONLIGHT_FONT, 26);
        if (font == NULL) {
            fprintf(stderr, "Could not load font: %s\n", TTF_GetError());
            return;
        }

        SDL_Color textColor = {255, 255, 255, 255};
        SDL_Surface *textSurface = TTF_RenderText_Blended(font, text, textColor);

        if(!textSurface) {
            fprintf(stderr, "Could not render text: %s\n", TTF_GetError());
            TTF_CloseFont(font);
            return;
        }

        SDL_Rect textRect;
        textRect.x = (rect.w - textSurface->w) / 2;
        textRect.y = rect.y + (rect.h - textSurface->h) / 2;
        textRect.w = textSurface->w;
        textRect.h = textSurface->h;

        SDL_BlitSurface(textSurface, NULL, ctx->menu_surface, &textRect);

        SDL_FreeSurface(textSurface);
        TTF_CloseFont(font);
    }

    SDL_Texture* menu_texture = SDL_CreateTextureFromSurface(ctx->renderer, ctx->menu_surface);
    SDL_RenderCopy(ctx->renderer, menu_texture, NULL, NULL);
    SDL_DestroyTexture(menu_texture);

    SDL_RenderPresent(ctx->renderer);
}


void sdl_unpair(SDLContext *ctx, const char *IPADDR) {
    char pairdone_path[128], cache_path[128], preload_path[128], command[256];
    char buffer[128];
    FILE *fp;

    snprintf(pairdone_path, sizeof(pairdone_path), "%s/config/pairdone", MOONLIGHT_DIR);
    snprintf(cache_path, sizeof(cache_path), "%s/.cache", MOONLIGHT_DIR);
    snprintf(preload_path, sizeof(preload_path), "%s/lib/libuuid.so", MOONLIGHT_DIR);

    is_file_exist_and_remove(pairdone_path);
    is_dir_exist_and_remove(cache_path);
    is_file_exist_and_remove("/tmp/launch");

    snprintf(command, sizeof(command), "LD_PRELOAD=%s moonlight unpair %s", preload_path, IPADDR);
    fp = popen(command, "r");
    if (fp == NULL) {
        printf("Problem getting fp\n");
        return;
    }

    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        printf("Received reply: %s", buffer); 

        if (strstr(buffer, "Successfully unpaired") != NULL) {
            ctx->state.unPairedNoti = 1;
            printf("Matched Successfully unpaired\n");
            break;
        } else if (strstr(buffer, "Failed to unpair to server") != NULL) {
            ctx->state.unPairedNoti = 2;
            printf("Matched Failed to unpair to server\n");
            break;
        }
}

    pclose(fp);
}

void sdl_tile(SDLContext *ctx, SDL_Surface* surface, int columns, int rows, int selected, int index, const char* text) {
    int screen_width = 640;
    int screen_height = 480;
    int padding = 20;
    int tile_gap = 20;
    int usable_width = screen_width - 2 * padding;
    int tile_width = (usable_width - (tile_gap * (columns - 1))) / columns;
    int tile_height = 100;
    int total_tile_height = (tile_height * rows) + (tile_gap * (rows - 1));
    int startX = padding;
    int startY = screen_height - total_tile_height - padding;
    int x = index % columns;
    int y = index / columns;
    
    SDL_Rect rect;
    rect.x = startX + x * (tile_width + tile_gap);
    rect.y = startY + y * (tile_height + tile_gap);
    rect.w = tile_width;
    rect.h = tile_height;
    Uint32 bg_color = (selected == index) ? SDL_MapRGB(surface->format, 144, 144, 144) : SDL_MapRGB(surface->format, 108, 108, 108);
    SDL_FillRect(surface, &rect, bg_color);

    SDL_Rect bandRectBottom;
    bandRectBottom.x = rect.x;
    bandRectBottom.y = rect.y + rect.h - 10;
    bandRectBottom.w = rect.w;
    bandRectBottom.h = 10;
    Uint32 band_color_bottom = (selected == index) ? SDL_MapRGB(surface->format, 225, 0, 0) : SDL_MapRGB(surface->format, 98, 98, 98);
    SDL_FillRect(surface, &bandRectBottom, band_color_bottom);

    SDL_Rect bandRectTop;
    bandRectTop.x = rect.x;
    bandRectTop.y = rect.y;
    bandRectTop.w = rect.w;
    bandRectTop.h = 5;
    Uint32 band_color_top = (selected == index) ? SDL_MapRGB(surface->format, 164, 164, 164) : SDL_MapRGB(surface->format, 138, 138, 138);
    SDL_FillRect(surface, &bandRectTop, band_color_top);

      if (TTF_Init() == -1) {
        fprintf(stderr, "Could not initialize SDL_ttf: %s\n", TTF_GetError());
        return;
    }
    
    TTF_Font *font = TTF_OpenFont(MOONLIGHT_FONT, 30);
    if (font == NULL) {
        fprintf(stderr, "Could not load font: %s\n", TTF_GetError());
        TTF_Quit();
        return;
    }

    SDL_Color textColor = {255, 255, 255, 0};

    SDL_Surface* textSurface = TTF_RenderText_Blended(font, text, textColor);

    SDL_Rect textRect;
    textRect.x = rect.x + (rect.w - textSurface->w) / 2;
    textRect.y = rect.y + (rect.h - textSurface->h) / 2;
    textRect.w = textSurface->w;
    textRect.h = textSurface->h;

    SDL_BlitSurface(textSurface, NULL, surface, &textRect);
    
    SDL_FreeSurface(textSurface);
    TTF_CloseFont(font);
    TTF_Quit();
}

int sdl_menu(SDLContext *ctx) {
    printf("Entering sdl_menu\n");

    SDL_Event event;
    int selected_item = 0;
    ctx->state.redraw = 1;
    ctx->state.redrawAll = 1;

    const char* menu_texts[6] = {"Stream", "Pair", "Unpair", " ", "Settings", "Exit"};
    const char* settings_texts[6] = {"Keybinds", "Mouse", "1", "1", "1", "Back"};

    while (1) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                return 1;
            }
            if (event.type == SDL_KEYDOWN) {
                if (ctx->state.inSettings) {
                    switch (event.key.keysym.sym) {
                        case SDLK_SPACE:
                        switch (selected_item) {
                                case 0:
                                    sdl_banner(ctx, "Not yet", "red");
                                    break;
                                case 1:
                                    sdl_banner(ctx, "Not yet", "red");
                                    break;
                                case 2:
                                    sdl_banner(ctx, "Not yet", "red");
                                    break;
                                case 3:
                                    sdl_banner(ctx, "Not yet", "red");
                                    break;
                                case 4:
                                    sdl_banner(ctx, "Not yet", "red");
                                    break;
                                case 5:
                                    ctx->state.inSettings = 0;
                                    selected_item = 0;
                                default:
                                    break;
                            }
                            printf("In Settings: Pressed SPACE\n");
                            break;
                        case SDLK_BACKSPACE:
                            printf("In Settings: Pressed B\n");
                            ctx->state.inSettings = 0;
                            ctx->state.redraw = 1;
                            ctx->state.redrawAll = 1;
                            break;
                        case SDLK_ESCAPE:
                            printf("In Settings: Pressed ESCAPE\n");
                            ctx->state.inSettings = 0;
                            ctx->state.redraw = 1;
                            break;
                        case SDLK_UP:
                            selected_item = (selected_item - COLUMNS + MAX_ITEMS) % MAX_ITEMS;
                            ctx->state.redraw = 1;
                            break;
                        case SDLK_DOWN:
                            selected_item = (selected_item + COLUMNS) % MAX_ITEMS;
                            ctx->state.redraw = 1;
                            break;
                        case SDLK_LEFT:
                            selected_item = (selected_item - 1 + MAX_ITEMS) % MAX_ITEMS;
                            ctx->state.redraw = 1;
                            break;
                        case SDLK_RIGHT:
                            selected_item = (selected_item + 1) % MAX_ITEMS;
                            ctx->state.redraw = 1;
                            break;
                        default:
                            break;
                } 
                } else {
                    switch (event.key.keysym.sym) {
                        case SDLK_SPACE:
                            switch (selected_item) {
                                case 0:
                                    return 0;
                                    break;
                                case 1:
                                    // sdl_pair();
                                    break;
                                case 2:
                                    sdl_unpair(ctx, "192.168.1.84"); // fix IP
                                    break;
                                case 3:
                                    sdl_banner(ctx, "Not active yet", "green");
                                    ctx->state.redraw = 0;
                                    ctx->state.redrawAll = 0;
                                    break;
                                case 4:
                                    ctx->state.redraw = 1;
                                    ctx->state.redrawAll = 1;
                                    selected_item = 0;
                                    ctx->state.inSettings = 1;
                                    break;
                                case 5:
                                    return 1;
                                default:
                                    break;
                            }
                            ctx->state.redraw = 1;
                            break;
                        case SDLK_BACKSPACE:
                            return 1;
                            break;
                        case SDLK_ESCAPE:
                            printf("Pressed ESCAPE\n");
                            break;
                        case SDLK_UP:
                            selected_item = (selected_item - COLUMNS + MAX_ITEMS) % MAX_ITEMS;
                            ctx->state.redraw = 1;
                            ctx->state.redrawAll = 1;
                            break;
                        case SDLK_DOWN:
                            selected_item = (selected_item + COLUMNS) % MAX_ITEMS;
                            ctx->state.redraw = 1;
                            ctx->state.redrawAll = 1;
                            break;
                        case SDLK_LEFT:
                            selected_item = (selected_item - 1 + MAX_ITEMS) % MAX_ITEMS;
                            ctx->state.redraw = 1;
                            ctx->state.redrawAll = 1;
                            break;
                        case SDLK_RIGHT:
                            selected_item = (selected_item + 1) % MAX_ITEMS;
                            ctx->state.redraw = 1;
                            ctx->state.redrawAll = 1;
                            break;
                        default:
                            break;
                    }
                }
            }
        }
            
   
    if (ctx->state.redraw) {
            if (ctx->state.redrawAll) {
                sdl_base_ui(ctx);
                ctx->state.redrawAll = 0;
            }
        }

        if (!ctx->state.inSettings) {
            for (int i = 0; i < MAX_ITEMS; ++i) {
                sdl_tile(ctx, ctx->menu_surface, COLUMNS, ROWS, selected_item, i, menu_texts[i]);
            }
        } else {
            for (int i = 0; i < MAX_ITEMS; ++i) {
                sdl_tile(ctx, ctx->menu_surface, COLUMNS, ROWS, selected_item, i, settings_texts[i]);
            }
        }
        
        if (ctx->state.unPairedNoti == 2) {
            sdl_banner(ctx, "Failed to unpair....", "red");
            ctx->state.unPairedNoti = 0;
            ctx->state.redrawAll = 0;
            ctx->state.redraw = 0;
        } else if (ctx->state.unPairedNoti == 1) {
            sdl_banner(ctx, "Successfully unpaired!", "green");
            ctx->state.unPairedNoti = 0;
            ctx->state.redrawAll = 0;
            ctx->state.redraw = 0;
        } 
        
        
        if (ctx->state.noPairStart == 1) {
            sdl_banner(ctx, "This device is not paired!", "red");
            ctx->state.noPairStart = 0;
            ctx->state.redrawAll = 0;
            ctx->state.redraw = 0;
        }
        
        SDL_UpdateTexture(ctx->menu_texture, NULL, ctx->menu_surface->pixels, ctx->menu_surface->pitch);
        SDL_RenderCopy(ctx->renderer, ctx->menu_texture, NULL, NULL);
        SDL_RenderPresent(ctx->renderer);

        ctx->state.redraw = 0;
        ctx->state.redrawAll = 0;
        
        SDL_Delay(20);
    }

    unPair = 0;
    SDL_DestroyTexture(ctx->menu_texture);
    SDL_FreeSurface(ctx->menu_surface);
    TTF_Quit();
    printf("Destroyed menu_texture\n");
    return 0;
}

void sdl_splash(SDLContext *ctx) {
    printf("Entering sdl_menu\n");

    SDL_Event event;

    Uint32 rmask = 0xF800;
    Uint32 gmask = 0x07E0;
    Uint32 bmask = 0x001F;
    Uint32 amask = 0x0000;

    SDL_Surface* menu_surface = SDL_CreateRGBSurface(0, 640, 480, 16, rmask, gmask, bmask, amask);
    if (!menu_surface) {
        fprintf(stderr, "SDL: could not create surface - %s\n", SDL_GetError());
        exit(1);
    } else {
        printf("Successfully created menu_surface\n");
    }

    SDL_FillRect(menu_surface, NULL, SDL_MapRGB(menu_surface->format, 64, 64, 64));
    SDL_Texture* menu_texture = NULL;

    const char* image_path = SPLASH_PATH;
    int nextFrameNumber = 0;

    while (1) {
        char filename[256];
        snprintf(filename, sizeof(filename), "%s/frame%04d.png", image_path, nextFrameNumber);
        SDL_Surface* png_surface = IMG_Load(filename);

        if (!png_surface) {
            fprintf(stderr, "Could not load PNG image: %s\n", IMG_GetError());
            break;
        }

        SDL_BlitSurface(png_surface, NULL, menu_surface, NULL);
        SDL_FreeSurface(png_surface);

        if (!menu_texture) {
            menu_texture = SDL_CreateTextureFromSurface(ctx->renderer, menu_surface);
        } else {
            SDL_UpdateTexture(menu_texture, NULL, menu_surface->pixels, menu_surface->pitch);
        }

        nextFrameNumber++;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                printf("Received SDL_QUIT event\n");
                goto cleanup;
            }
            if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_a:
                        printf("Pressed A\n");
                        break;
                    case SDLK_b:
                        printf("Pressed B\n");
                        break;
                    case SDLK_ESCAPE:
                        printf("Pressed ESCAPE\n");
                        goto cleanup;
                }
            }
        }

        if (menu_texture) {
            SDL_RenderCopy(ctx->renderer, menu_texture, NULL, NULL);
            SDL_RenderPresent(ctx->renderer);
        }

        SDL_Delay(30);
    }

cleanup:
    SDL_FreeSurface(menu_surface);
    if (menu_texture) {
        SDL_DestroyTexture(menu_texture);
    }
    printf("Destroyed menu_texture\n");
}

void sdl_loop(SDLContext *ctx) {
  SDL_Event event;

  SDL_SetRelativeMouseMode(SDL_TRUE);

  while(!done && SDL_WaitEvent(&event)) {
    switch (sdlinput_handle_event(ctx->window, &event)) {
    case SDL_QUIT_APPLICATION:
      done = true;
      break;
    case SDL_TOGGLE_FULLSCREEN:
      fullscreen_flags ^= SDL_WINDOW_FULLSCREEN;
      SDL_SetWindowFullscreen(ctx->window, fullscreen_flags);
      break;
    case SDL_MOUSE_GRAB:
      SDL_ShowCursor(SDL_ENABLE);
      SDL_SetRelativeMouseMode(SDL_TRUE);
      break;
    case SDL_MOUSE_UNGRAB:
      SDL_SetRelativeMouseMode(SDL_FALSE);
      SDL_ShowCursor(SDL_DISABLE);
      break;
    default:
      if (event.type == SDL_QUIT)
        done = true;
      else if (event.type == SDL_USEREVENT) {
        if (event.user.code == SDL_CODE_FRAME) {
          if (++sdlCurrentFrame <= sdlNextFrame - SDL_BUFFER_FRAMES) {
            //Skip frame
          } else if (SDL_LockMutex(mutex) == 0) {
            Uint8** data = ((Uint8**) event.user.data1);
            int* linesize = ((int*) event.user.data2);
            SDL_UpdateYUVTexture(ctx->bmp, NULL, data[0], linesize[0], data[1], linesize[1], data[2], linesize[2]);
            SDL_UnlockMutex(mutex);
            SDL_RenderClear(ctx->renderer);
            SDL_RenderCopy(ctx->renderer, ctx->bmp, NULL, NULL);
            SDL_RenderPresent(ctx->renderer);
          } else
            fprintf(stderr, "Couldn't lock mutex\n");
        }
      }
    }
  }

  SDL_DestroyWindow(ctx->window);
  SDL_Quit();
}

#endif /* HAVE_SDL */
