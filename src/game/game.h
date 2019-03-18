#ifndef GAME_H
#define GAME_H
#include <engine/std.h>
#include <core/render_context.h>
#include <game/memory.h>

class InputManager;
struct Assets;
struct AudioEngine;

#define GAME_INIT(name) void name(Platform *platform, MemoryStore *mem_store, RenderContext *render_context, Assets *assets, AudioEngine *audio)
typedef GAME_INIT(GameInitFunc);

GAME_INIT(gameInitStub) {
	
}


#define GAME_UPDATE(name) void name(Platform *platform, MemoryStore *mem_store, InputManager *input, f32 delta, PlatformWindow *window, Assets *assets)
typedef GAME_UPDATE(GameUpdateFunc);

GAME_UPDATE(gameUpdateStub) {
	
}


#define GAME_RENDER(name) void name(Platform *platform, MemoryStore *mem_store, PlatformWindow *window, RenderContext *render_context, InputManager *input, Assets *assets, f32 delta)
typedef GAME_RENDER(GameRenderFunc);

GAME_RENDER(gameRenderStub) {
	
}

#endif // GAME_H