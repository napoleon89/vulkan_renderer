#include <stdio.h>
#include <string.h>
#include <engine/std.h>
#include <engine/timer.cpp>
#include <stdlib.h>
#include <engine/math.cpp>
#include <string>
#include <game/game.h>
#include <core/platform.h>
#include <engine/input.cpp>
#include <core/render_context.h>
#define STB_IMAGE_IMPLEMENTATION
#include <core/stb_image.h>
#include <engine/audio.h>
#include <game/assets.cpp>
#include <game/memory.h>
#include <engine/audio.cpp>
#include <SDL2/SDL.h>
#include <vulkan/vulkan.h>
#include <SDL2/SDL_vulkan.h>
#include <core/vulkan_renderer.cpp>
#define TINYOBJLOADER_IMPLEMENTATION
#include <core/tiny_obj_loader.h>

struct GameCode {
	void *game_code_dll;
	GameInitFunc *init;
	GameUpdateFunc *update;
	GameRenderFunc *render;
	FileTime last_write_time;
	bool is_valid;
};

internal_func GameCode loadGameCode(Platform *platform, const char *source_dll_name, const char *temp_dll_name) {
	GameCode result = {};
	platform->copyFile((char *)source_dll_name, (char *)temp_dll_name);
	result.last_write_time = platform->getLastWriteTime((char *)source_dll_name);
	result.game_code_dll = platform->loadLibrary(temp_dll_name);
	
	if(result.game_code_dll) {
		result.init = (GameInitFunc *)platform->loadFunction(result.game_code_dll, "gameInit");
		result.update = (GameUpdateFunc *)platform->loadFunction(result.game_code_dll, "gameUpdate");
		result.render = (GameRenderFunc *)platform->loadFunction(result.game_code_dll, "gameRender");
		
		result.is_valid = (result.init != 0 && result.update != 0 && result.render != 0);
	}
	
	if(!result.is_valid) {
		result.init = gameInitStub;
		result.update = gameUpdateStub;
		result.render = gameRenderStub;	
	}
	
	return result;
}

internal_func void unloadGameCode(Platform *platform, GameCode *game_code) {
	if(game_code->game_code_dll) {
		platform->unloadLibrary(game_code->game_code_dll);
		game_code->game_code_dll = 0;
	}
	
	game_code->is_valid = false;
	game_code->init = gameInitStub;
	game_code->update = gameUpdateStub;
	game_code->render = gameRenderStub;	
}

int main(int arg_count, char *args[]) {
	
	Platform platform = {};
	if(!platform.init()) {
		platform.error("Couldn't init platform");
	}
	
	char *base_path = platform.getExePath();
	std::string game_dll_name = std::string(base_path) + "game.dll";
	std::string temp_game_dll_name = std::string(base_path) + "game_temp.dll";
	
	u32 window_width = 1600;
	u32 window_height = 900;
	
	PlatformWindow window = platform.createWindow("Pawprint Engine", window_width, window_height, true, true);
	
	
	if(window.handle == 0) {
		platform.error("Couldn't create window");
	}
	s32 refresh_rate = 60;
	
	
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string err;
	
	std::vector<Vertex> vertices;
	std::vector<u32> indices;
	
	if(!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, "data/models/chalet.obj")) {
		platform.error((char *)err.c_str());
	}
	
	for(const tinyobj::shape_t &shape : shapes) {
		for(const auto &index : shape.mesh.indices) {
			Vertex vertex = {};
			
			vertex.pos = {
				attrib.vertices[3 * index.vertex_index + 0],
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2],
			};
			
			vertex.uv = {
				attrib.texcoords[2 * index.texcoord_index + 0],
				1.0f - attrib.texcoords[2 * index.texcoord_index + 1],
			};
			
			vertex.color = Vec3(1.0f);
			
			vertices.push_back(vertex);
			indices.push_back((u32)indices.size());
		}
	}
	
	VulkanRenderer renderer;
	renderer.vertices = vertices.data();
	renderer.vertex_count = (u32)vertices.size();
	
	renderer.indices = indices.data();
	renderer.index_count = (u32)indices.size();
	
	renderer.init(&platform, &window);	
	
	AudioEngine audio_engine;
	audio_engine.init();
	
	Timer frame_timer = Timer(&platform);
	bool running = true;
	
	MemoryStore mem_store = {};
	mem_store.game_memory = {0, Megabytes(8)};
	mem_store.asset_memory = {0, Megabytes(8)};
	mem_store.frame_memory = {0, Megabytes(2)};
	
	u64 mem_total_size = 0;
	for(int i = 0; i < MEMORY_STORE_COUNT; i++) {
		mem_total_size += mem_store.blocks[i].size;
	}
	
	mem_store.memory = (void *)platform.alloc(mem_total_size);
	memset(mem_store.memory, 0, mem_total_size); // NOTE(nathan): this might need removing for performance??
	u8 *byte_walker = (u8 *)mem_store.memory;
	for(int i = 0; i < MEMORY_STORE_COUNT; i++) {
		mem_store.blocks[i].memory = (void *)byte_walker;
		byte_walker += mem_store.blocks[i].size;
	}
	
	
	f32 target_seconds_per_frame = 1.0f / (f32)refresh_rate;
	f32 delta = target_seconds_per_frame;

	Assets *game_assets = (Assets *)mem_store.asset_memory.memory;
	Assets::db = game_assets;
	
	platform.getDirectoryContents();
	
	GameCode game_code = loadGameCode(&platform, game_dll_name.c_str(), temp_game_dll_name.c_str());
	game_code.init(&platform, &mem_store, 0, game_assets, &audio_engine);
	
	u32 current_frame = 0;
	
	InputManager input(&window);
	while(running) {
		renderer.startFrame();
		
		frame_timer.start(&platform);
		
		bool requested_to_quit = false;
		platform.processEvents(&window, requested_to_quit);
		running = !requested_to_quit;
		input.processKeys(&platform);
		
		FileTime new_dll_write_time = platform.getLastWriteTime((char *)game_dll_name.c_str());
		if(platform.compareFileTime(&new_dll_write_time, &game_code.last_write_time) != 0) {
			unloadGameCode(&platform, &game_code);
			printf("Reloading game code\n");
			game_code = loadGameCode(&platform, game_dll_name.c_str(), temp_game_dll_name.c_str());
		}
		
		
		if(input.isKeyDownOnce(Key::Escape)) {
			running = false;
		}
				
		if(input.isKeyDownOnce(Key::F11)) {
			platform.setWindowFullscreen(&window, platform.isWindowFullscreen(&window));
		}
		
		game_code.update(&platform, &mem_store, &input, delta, &window, game_assets);
		
		renderer.renderFrame(&platform, &window, delta);
		
		game_code.render(&platform, &mem_store, &window, 0, &input, game_assets, delta);
		
		u64 work_counter = frame_timer.getWallClock(&platform);
		f32 work_seconds_elapsed = frame_timer.getSecondsElapsed(&platform);
		
		f32 seconds_elapsed_for_frame = work_seconds_elapsed;
		if(seconds_elapsed_for_frame < target_seconds_per_frame)
		{
			u32 sleep_ms = (u32)((1000.0f * (target_seconds_per_frame - seconds_elapsed_for_frame)) - 1);
			if(sleep_ms > 0)
				platform.sleepMS(sleep_ms);
			while(seconds_elapsed_for_frame < target_seconds_per_frame)
			{
				seconds_elapsed_for_frame = frame_timer.getSecondsElapsed(&platform);
			}
		}
		else
		{
			// missed frame
			printf("Missed frame\n");
		}
		u64 end_counter = frame_timer.getWallClock(&platform);
		delta = frame_timer.getSecondsElapsed(&platform);
		input.endFrame();
		
		platform.setWindowTitle(&window, formatString("%.3fms/frame", delta * 1000.0f));
		
		renderer.endFrame();
	}
	
	renderer.cleanup();
	audio_engine.uninit();
	unloadGameCode(&platform, &game_code);
	platform.destroyWindow(&window);
	
	
	platform.uninit();
	return 0;
}