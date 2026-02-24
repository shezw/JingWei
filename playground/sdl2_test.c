/**
    -----------------------------------------------------------

 	Project JingWei
 	playground sdl2_test.c    2026/02/24
 	
 	@link    : https://github.com/shezw/jingwei
 	@author	 : shezw
 	@email	 : hello@shezw.com

    -----------------------------------------------------------
*/

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>

#define WINDOW_WIDTH  800
#define WINDOW_HEIGHT 480

int main(int argc, char *argv[]) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    // Create window
    SDL_Window *window = SDL_CreateWindow("JingWei SDL2 Experiment",
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          WINDOW_WIDTH, WINDOW_HEIGHT,
                                          SDL_WINDOW_SHOWN);
    if (window == NULL) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Create renderer (hardware accelerated if possible)
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Create a texture for pixel manipulation
    SDL_Texture *texture = SDL_CreateTexture(renderer,
                                             SDL_PIXELFORMAT_ARGB8888,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             WINDOW_WIDTH, WINDOW_HEIGHT);
    if (texture == NULL) {
        printf("Texture could not be created! SDL_Error: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    bool quit = false;
    SDL_Event e;
    
    uint8_t start_r = 0, start_g = 0, start_b = 0;
    uint32_t *pixels = NULL;
    int pitch = 0;

    // Main loop
    while (!quit) {
        // Handle events
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            } else if (e.type == SDL_MOUSEMOTION) {
                printf("Mouse moved: (%d, %d)\n", e.motion.x, e.motion.y);
            } else if (e.type == SDL_MOUSEBUTTONDOWN) {
                printf("Mouse button down: button=%d at (%d, %d)\n", 
                       e.button.button, e.button.x, e.button.y);
            } else if (e.type == SDL_MOUSEBUTTONUP) {
                printf("Mouse button up: button=%d at (%d, %d)\n", 
                       e.button.button, e.button.x, e.button.y);
            }
        }

        // Lock texture for manipulation
        if (SDL_LockTexture(texture, NULL, (void**)&pixels, &pitch) == 0) {
            uint8_t r = start_r;
            uint8_t g = start_g;
            uint8_t b = start_b;

            // Fill pixels row by row
            for (int y = 0; y < WINDOW_HEIGHT; ++y) {
                uint32_t color = (255 << 24) | (r << 16) | (g << 8) | b; // ARGB8888
                
                // Set the whole row to this color
                for (int x = 0; x < WINDOW_WIDTH; ++x) {
                    pixels[y * (pitch / 4) + x] = color;
                }

                // Increment color per line as requested: r+=2, g+=5, b+=7
                r += 2;
                g += 5;
                b += 7;
            }
            SDL_UnlockTexture(texture);
        }

        // Animate the starting color to make the pattern move
        start_r += 1;
        start_g += 1;
        start_b += 1;

        // Render
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        // Delay ~10ms
        SDL_Delay(10);
    }

    // Cleanup
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
