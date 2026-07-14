#define SDL_MAIN_USE_CALLBACKS  // This is necessary for the new callbacks API. To use the legacy API, don't define this. 
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_init.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_image/SDL_image.h>

const uint32_t windowStartWidth = 400;
const uint32_t windowStartHeight = 400;

struct AppContext {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* messageTex, *imageTex;
    SDL_FRect messageDest;
    SDL_AudioDeviceID audioDevice;
    MIX_Track* track;
    SDL_AppResult app_quit;
};

#define APP_FAIL do { \
    SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Error %s at %s:%d", SDL_GetError(), SDL_FILE,  SDL_LINE); \
    return SDL_APP_FAILURE; \
} while(SDL_NULL_WHILE_LOOP_CONDITION)

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    // init the library, here we make a window so we only need the Video capabilities.
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)){
        APP_FAIL;
    }
    if (!TTF_Init()) {
        APP_FAIL;
    }
    if (!MIX_Init()) {
        APP_FAIL;
    }
    SDL_Window* window = SDL_CreateWindow("SDL Minimal Sample", windowStartWidth, windowStartHeight, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window){
        APP_FAIL;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer){
        APP_FAIL;
    }
    
    // load the font
#if __ANDROID__
    const char *basePath = ""; // on Android we do not want to use basepath. Instead, assets are available at the root directory.
#else
    const char *basePath = SDL_GetBasePath();
     if (!basePath) {
        APP_FAIL;
    }
#endif
    TTF_Font* font = NULL;
    {
        char *fontPath = NULL;
        if (SDL_asprintf(&fontPath, "%s/%s", basePath, "Inter-VariableFont.ttf") < 0) {
            APP_FAIL;
        }
        font = TTF_OpenFont(fontPath, 36);
        if (!font) {
            SDL_free(fontPath);
            APP_FAIL;
        }
        SDL_free(fontPath);
    }

    // render the font to a surface
    const char text[] = "Hello SDL!";
    SDL_Surface* surfaceMessage = TTF_RenderText_Solid(font, text, sizeof(text)-1, (SDL_Color){ 255,255,255,255 });

    // make a texture from the surface
    SDL_Texture* messageTex = SDL_CreateTextureFromSurface(renderer, surfaceMessage);

    // we no longer need the font or the surface, so we can destroy those now.
    TTF_CloseFont(font);
    SDL_DestroySurface(surfaceMessage);

    // load the SVG
    SDL_Surface *svg_surface = NULL;
    {
        char *imagePath = NULL;
        if (SDL_asprintf(&imagePath, "%s/%s", basePath, "gs_tiger.svg") < 0) {
            APP_FAIL;
        }
        svg_surface = IMG_Load(imagePath);
        SDL_free(imagePath);
    }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, svg_surface);
    SDL_DestroySurface(svg_surface);
    
    // get the on-screen dimensions of the text. this is necessary for rendering it
    SDL_PropertiesID messageTexProps = SDL_GetTextureProperties(messageTex);
    SDL_FRect text_rect = {
            .x = 0,
            .y = 0,
            .w = SDL_GetNumberProperty(messageTexProps, SDL_PROP_TEXTURE_WIDTH_NUMBER, 0),
            .h = SDL_GetNumberProperty(messageTexProps, SDL_PROP_TEXTURE_HEIGHT_NUMBER, 0)
    };

    // init SDL Mixer
    MIX_Mixer* mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (!mixer) {
        APP_FAIL;
    }
    
    MIX_Track *mixerTrack = MIX_CreateTrack(mixer);

    // load the music
    MIX_Audio *music = NULL;
    {
        char *musicPath = NULL;
        if (SDL_asprintf(&musicPath, "%s/%s", basePath, "the_entertainer.ogg") < 0) {
            APP_FAIL;
        }
        music = MIX_LoadAudio(mixer,musicPath,false);
        if (!music) {
            SDL_free(musicPath);
            APP_FAIL;
        }
        SDL_free(musicPath);
    }

    // play the music (does not loop)
    MIX_SetTrackAudio(mixerTrack, music);
    MIX_PlayTrack(mixerTrack, 0);
    
    // print some information about the window
    SDL_ShowWindow(window);
    {
        int width, height, bbwidth, bbheight;
        SDL_GetWindowSize(window, &width, &height);
        SDL_GetWindowSizeInPixels(window, &bbwidth, &bbheight);
        SDL_Log("Window size: %ix%i", width, height);
        SDL_Log("Backbuffer size: %ix%i", bbwidth, bbheight);
        if (width != bbwidth){
            SDL_Log("This is a highdpi environment.");
        }
    }

    // set up the application data
    {
        struct AppContext* app = SDL_calloc(1, sizeof(struct AppContext));
        if (!app) {
            APP_FAIL;
        }
        *app = (struct AppContext){
           .window = window,
           .renderer = renderer,
           .messageTex = messageTex,
           .imageTex = tex,
           .messageDest = text_rect,
           .track = mixerTrack,
           .app_quit = SDL_APP_CONTINUE
        };
        *appstate = app;
    }
    
    SDL_SetRenderVSync(renderer, -1);   // enable vsync
    
    SDL_Log("Application started successfully!");

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event* event) {
    struct AppContext* app = (struct AppContext*)appstate;
    
    if (event->type == SDL_EVENT_QUIT) {
        app->app_quit = SDL_APP_SUCCESS;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
    struct AppContext *app = (struct AppContext*)appstate;

    // draw a color
    float time = SDL_GetTicks() / 1000.f;
    float red = (SDL_sinf(time) + 1) / 2.0 * 255;
    float green = (SDL_sinf(time / 2) + 1) / 2.0 * 255;
    float blue = (SDL_sinf(time) * 2 + 1) / 2.0 * 255;
    
    SDL_SetRenderDrawColor(app->renderer, red, green, blue, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(app->renderer);

    // Renderer uses the painter's algorithm to make the text appear above the image, we must render the image first.
    SDL_RenderTexture(app->renderer, app->imageTex, NULL, NULL);
    SDL_RenderTexture(app->renderer, app->messageTex, NULL, &app->messageDest);

    SDL_RenderPresent(app->renderer);

    return app->app_quit;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    struct AppContext *app = (struct AppContext*)appstate;
    if (app) {
        SDL_DestroyRenderer(app->renderer);
        SDL_DestroyWindow(app->window);
        
        // prevent the music from abruptly ending.
        MIX_StopTrack(app->track, MIX_TrackMSToFrames(app->track, 1000));
        SDL_Delay(1000); // ms
        //Mix_FreeMusic(app->music); // this call blocks until the music has finished fading
        SDL_CloseAudioDevice(app->audioDevice);

        SDL_free(app);
    }
    TTF_Quit();
    MIX_Quit();

    SDL_Log("Application quit successfully!");
    SDL_Quit();
}
