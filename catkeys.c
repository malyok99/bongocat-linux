#define _GNU_SOURCE
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <libevdev/libevdev.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define KEY_CNT 256

volatile int current_image = 0;
pthread_mutex_t image_mutex = PTHREAD_MUTEX_INITIALIZER;

char key_states[KEY_CNT] = {0}; // 1 - pressed, 0 - released
pthread_mutex_t key_mutex = PTHREAD_MUTEX_INITIALIZER;

struct evdev_reader_args {
    const char* device_path;
};

void* evdev_thread(void* arg) {
    struct evdev_reader_args *args = (struct evdev_reader_args*)arg;

    int fd = open(args->device_path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "Failed to open %s: %s\n", args->device_path, strerror(errno));
        pthread_exit(NULL);
    }

    struct libevdev *dev = NULL;
    if (libevdev_new_from_fd(fd, &dev) < 0) {
        fprintf(stderr, "Failed to init libevdev on %s\n", args->device_path);
        close(fd);
        pthread_exit(NULL);
    }

    struct input_event ev;
    int rc;

    while (1) {
        rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
        if (rc == 0) {
            if (ev.type == EV_KEY && ev.code < KEY_CNT) {
                pthread_mutex_lock(&key_mutex);
                key_states[ev.code] = (ev.value != 0);
                pthread_mutex_unlock(&key_mutex);
            }
        } else if (rc != -EAGAIN) {
            fprintf(stderr, "libevdev error: %d\n", rc);
            break;
        }
        usleep(1000);
    }

    libevdev_free(dev);
    close(fd);
    pthread_exit(NULL);
}

SDL_Texture* load_texture(SDL_Renderer* renderer, const char* file) {
    SDL_Surface* surface = IMG_Load(file);
    if (!surface) {
        fprintf(stderr, "IMG_Load error for %s: %s\n", file, IMG_GetError());
        return NULL;
    }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return tex;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: sudo %s /dev/input/eventX\n", argv[0]);
        return 1;
    }

    if (geteuid() != 0) {
        fprintf(stderr, "This program must be run as root.\n");
        return 1;
    }

    const char* devpath = argv[1];

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
        return 1;
    }

    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        fprintf(stderr, "IMG_Init error\n");
        SDL_Quit();
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("BongoCat Global Keyboard",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          WINDOW_WIDTH, WINDOW_HEIGHT,
                                          SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow error: %s\n", SDL_GetError());
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
                                                SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    const char* images[4] = {"0.png", "1.png", "2.png", "3.png"};
    SDL_Texture* textures[4] = {NULL, NULL, NULL, NULL};

    for (int i = 0; i < 4; i++) {
        textures[i] = load_texture(renderer, images[i]);
        if (!textures[i]) {
            fprintf(stderr, "Failed to load %s\n", images[i]);
            for (int j = 0; j < i; j++) SDL_DestroyTexture(textures[j]);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            IMG_Quit();
            SDL_Quit();
            return 1;
        }
    }

    pthread_t tid;
    struct evdev_reader_args args = {devpath};
    if (pthread_create(&tid, NULL, evdev_thread, &args) != 0) {
        fprintf(stderr, "Failed to create input thread\n");
        for (int i = 0; i < 4; i++) SDL_DestroyTexture(textures[i]);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    uint32_t last_key_time = 0;
    int last_image = 0;
    const int idle_timeout = 50; // ms

    int running = 1;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
        }

        int left_pressed = 0, right_pressed = 0, space_pressed = 0;

        pthread_mutex_lock(&key_mutex);
        for (int i = 0; i < KEY_CNT; i++) {
            if (!key_states[i]) continue;

            if (i == KEY_SPACE) space_pressed = 1;

            // Right half of the keyboard keys:
            switch(i) {
                case KEY_7: case KEY_8: case KEY_9: case KEY_0:
                case KEY_MINUS: case KEY_EQUAL: case KEY_BACKSPACE:
                case KEY_Y: case KEY_U: case KEY_I: case KEY_O: case KEY_P:
                case KEY_LEFTBRACE: case KEY_RIGHTBRACE: case KEY_BACKSLASH:
                case KEY_H: case KEY_J: case KEY_K: case KEY_L:
                case KEY_SEMICOLON: case KEY_APOSTROPHE: case KEY_ENTER:
                case KEY_B: case KEY_N: case KEY_M:
                case KEY_COMMA: case KEY_DOT: case KEY_SLASH:
                case KEY_RIGHTSHIFT:
                    right_pressed = 1;
                    break;
                default:
                    left_pressed = 1;
                    break;
            }
        }
        pthread_mutex_unlock(&key_mutex);

        int new_image = 0;
        if (space_pressed) new_image = 3;
        else if (left_pressed && right_pressed) new_image = 3;
        else if (right_pressed) new_image = 2;
        else if (left_pressed) new_image = 1;
        else new_image = 0;

        if (new_image != 0) {
            last_key_time = SDL_GetTicks();
            last_image = new_image;
        }
        if (SDL_GetTicks() - last_key_time > idle_timeout) {
            new_image = 0;
        }

        pthread_mutex_lock(&image_mutex);
        current_image = new_image;
        pthread_mutex_unlock(&image_mutex);

        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, textures[new_image], NULL, NULL);
        SDL_RenderPresent(renderer);

        SDL_Delay(10);
    }

    pthread_cancel(tid);
    pthread_join(tid, NULL);

    for (int i = 0; i < 4; i++) SDL_DestroyTexture(textures[i]);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();

    return 0;
}
