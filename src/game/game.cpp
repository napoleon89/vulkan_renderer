#include <game/game.h>
#include <engine/std.h>
#include <stdlib.h>
#include <stdio.h>
#include <engine/math.cpp>
#include <engine/input.cpp>
#include <float.h>
#include <string.h>
#include <vector>
#include <engine/allocators.cpp>
#include <game/assets.cpp>

global_variable StackAllocator* g_frame_stack;

struct Globals {
};

struct GameState {
	
	void init(Platform *platform, RenderContext *render_context, Assets *assets, AudioEngine *audio) {
	}
	
	void update(InputManager *input, f32 delta, Platform *platform, PlatformWindow *window, Assets *assets) {
		
	}
	
	void render(PlatformWindow *window, RenderContext *render_context, InputManager *input, Assets *assets, f32 delta) {
		
	}
};
GAME_INIT(gameInit) {
	g_frame_stack = (StackAllocator *)mem_store->frame_memory.memory;
	u64 new_frame_size = mem_store->frame_memory.size - sizeof(StackAllocator);
	g_frame_stack->initialize((void *)((u8 *)mem_store->frame_memory.memory + sizeof(StackAllocator)), new_frame_size);
	Assets::db = assets;
	
	u64 memory_size = sizeof(GameState);
	
	Assert(memory_size <= mem_store->game_memory.size);	
	GameState *game = (GameState *)mem_store->game_memory.memory;
	
	game->init(platform, render_context, assets, audio);
	
}

GAME_UPDATE(gameUpdate) {
	GameState *game = (GameState *)mem_store->game_memory.memory;
	g_frame_stack = (StackAllocator *)mem_store->frame_memory.memory;
	Assets::db = assets;
	g_frame_stack->clear();
	game->update(input, delta, platform, window, assets);
}



GAME_RENDER(gameRender) {
	GameState *game = (GameState *)mem_store->game_memory.memory;
	Assets::db = assets;
	game->render(window, render_context, input, assets, delta);
}