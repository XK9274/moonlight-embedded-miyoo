#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <png.h>
#include "../sdl.h"

SDL_Texture* nextFrame = NULL;
int nextFrameNumber = 0;
const char* image_path;
int debugging = 0;

extern SDL_Renderer *renderer;
extern SDL_Window *window;

int load_next_frame(void* data) {
    if (debugging) {
        fprintf(stderr, "Entering load_next_frame\n");
    }
    char filename[256];
    snprintf(filename, sizeof(filename), "%s/frame%04d.png", "/mnt/SDCARD/App/moonlight/splash", nextFrameNumber);

    FILE *fp = fopen(filename, "rb");
    if(!fp) {
        fprintf(stderr, "File %s could not be opened for reading\n", filename);
        return 1;
    }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info_ptr = png_create_info_struct(png_ptr);
    png_init_io(png_ptr, fp);
    png_read_info(png_ptr, info_ptr);

    int width = png_get_image_width(png_ptr, info_ptr);
    int height = png_get_image_height(png_ptr, info_ptr);

    png_read_update_info(png_ptr, info_ptr);
    int rowbytes = png_get_rowbytes(png_ptr, info_ptr);
    png_byte* image_data;
    image_data = malloc(rowbytes * height * sizeof(png_byte));
    png_bytep* row_pointers = malloc(height * sizeof(png_bytep));

    for(int y = 0; y < height; y++) {
        row_pointers[height - 1 - y] = image_data + y * rowbytes;
    }

    png_read_image(png_ptr, row_pointers);
    fclose(fp);

    SDL_Surface* frame = SDL_CreateRGBSurfaceFrom((void*)image_data, width, height,
                                                  32, rowbytes,
                                                  0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);

    nextFrame = SDL_CreateTextureFromSurface(renderer, frame);

    SDL_FreeSurface(frame);
    free(image_data);
    free(row_pointers);

    if (debugging) {
        fprintf(stderr, "Exiting load_next_frame\n");
    }
    return 0;
}

int show_splash() {
    int frameDuration = 40;

    nextFrameNumber = 1;

    SDL_Thread* thread = SDL_CreateThread(load_next_frame, "load_next_frame", NULL);
    if (thread == NULL) {
    fprintf(stderr, "Failed to create thread: %s\n", SDL_GetError());
    } else {
    fprintf(stderr, "Thread created successfully.\n");
    }

    int i = 1;
    while (1) {
    SDL_WaitThread(thread, NULL);

    if (nextFrame == NULL) {
    break;
    }

    SDL_RenderCopy(renderer, nextFrame, NULL, NULL);
    SDL_RenderPresent(renderer);

    SDL_DestroyTexture(nextFrame);
    nextFrame = NULL;

    nextFrameNumber = ++i;
    thread = SDL_CreateThread(load_next_frame, "load_next_frame", NULL);
    SDL_Delay(frameDuration);
    }

    return 0;
}