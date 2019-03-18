#include <windows.h>
#include <core/platform.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <engine/std.h>

global_variable u32 g_sdl_keys[] = {
	SDLK_0, SDLK_1, SDLK_2, SDLK_3, SDLK_4, SDLK_5, SDLK_6, SDLK_7, SDLK_8, SDLK_9, 
	SDLK_a, SDLK_b, SDLK_c, SDLK_d, SDLK_e, SDLK_f, SDLK_g, SDLK_h, SDLK_i, SDLK_j, 
	SDLK_k, SDLK_l, SDLK_m, SDLK_n, SDLK_o, SDLK_p, SDLK_q, SDLK_r, SDLK_s, SDLK_t, 
	SDLK_u, SDLK_v, SDLK_w, SDLK_x, SDLK_y, SDLK_z,
	SDLK_KP_0, SDLK_KP_1, SDLK_KP_2, SDLK_KP_3, SDLK_KP_4, SDLK_KP_5, SDLK_KP_6, SDLK_KP_7, SDLK_KP_8, SDLK_KP_9,
	SDLK_F1, SDLK_F2, SDLK_F3, SDLK_F4, SDLK_F5, SDLK_F6, SDLK_F7, SDLK_F8, SDLK_F9, SDLK_F10, SDLK_F11, SDLK_F12, 
	SDLK_F13, SDLK_F14, SDLK_F15, SDLK_F16, SDLK_F17, SDLK_F18, SDLK_F19, SDLK_F20, SDLK_F21, SDLK_F22, SDLK_F23, SDLK_F24,
	SDLK_LEFT, SDLK_RIGHT, SDLK_UP, SDLK_DOWN, SDLK_RCTRL, SDLK_LCTRL, SDLK_LSHIFT, SDLK_RSHIFT, SDLK_ESCAPE, SDLK_RETURN, SDLK_TAB, SDLK_LALT, SDLK_RALT,
	SDLK_PAGEUP, SDLK_PAGEDOWN, SDLK_AC_HOME, SDLK_END, SDLK_DELETE, SDLK_BACKSPACE, SDLK_LGUI, SDLK_RGUI
};

global_variable u8 *g_keyboard_state;
global_variable f32 g_mouse_wheel = 0.0f;

bool Platform::init() {
	bool result = SDL_Init(SDL_INIT_EVERYTHING) == 0;
	if(result) {
		g_keyboard_state = (u8 *)SDL_GetKeyboardState(0); // NOTE(nathan): pointer should be valid for app lifetime (according to docs);
	}
	return result;
}

void Platform::uninit() {
	SDL_Quit();	
}


void Platform::copyFile(char *a, char *b) {
	CopyFile(a, b, FALSE);
}

FileTime Platform::getLastWriteTime(char *filename) {
	FileTime last_write_time = {0};
	WIN32_FIND_DATA find_data;
	HANDLE find_handle = FindFirstFileA(filename, &find_data);
	if(find_handle != INVALID_HANDLE_VALUE) {
		last_write_time.low_date_time = find_data.ftLastWriteTime.dwLowDateTime;
		last_write_time.high_date_time = find_data.ftLastWriteTime.dwHighDateTime;
		FindClose(find_handle);
	}
	
	return last_write_time;
}

u32 Platform::compareFileTime(FileTime *a, FileTime *b) {
	FILETIME f;
	f.dwLowDateTime = a->low_date_time;
	f.dwHighDateTime = a->high_date_time;
	FILETIME g;
	g.dwLowDateTime = b->low_date_time;
	g.dwHighDateTime = b->high_date_time;
	return CompareFileTime(&f, &g);
}


void Platform::getDirectoryContents() {
	
}

FileData Platform::readEntireFile(const char *filename) { 
	FileData result = {0};
		
	SDL_RWops *diffuse_file = SDL_RWFromFile(filename, "rb");
	if(diffuse_file) {
		
		result.size = (u64)SDL_RWsize(diffuse_file);
		result.contents = (char *)SDL_malloc(result.size);
		
		SDL_RWread(diffuse_file, result.contents, 1, result.size);
		SDL_RWclose(diffuse_file);
	}
	else {
		error(formatString("Can't read %s", filename));
	}
	
	return result;	
}

void Platform::writeStructureToFile(const char *filename, void *structure, s32 size) {
	SDL_RWops *file = SDL_RWFromFile(filename, "wb");
	if(file) {
		if(SDL_RWwrite(file, structure, 1, size) != size) {
			error("Failed to write structure");
		}
		SDL_RWclose(file);
	} else {
		error("Couldn't open file for writing");
	}
}

void *Platform::openFileForWriting(const char *filename) {
	SDL_RWops *file = SDL_RWFromFile(filename, "wb");
	void *result = 0;
	if(file) {
		result = file;
	} else {
		error("Couldn't open file for writing");
		result = 0;
	}
	return result;	
}

void Platform::closeOpenFile(void *file) {
	if(file) {
		SDL_RWclose((SDL_RWops *)file);
	}
}

void Platform::writeToFile(void *file, void *structure, s32 size) {
	if(file) {
		if(SDL_RWwrite((SDL_RWops *)file, structure, 1, size) != size) {
			error("Failed to write structure");
		}
	}	
}

void *Platform::alloc(u64 size) {
	return SDL_malloc(size);	
}
void Platform::free(void *data) {
	return SDL_free(data);	
}
void Platform::error(char *err) {
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", err, 0);	
}

PlatformWindow Platform::createWindow(const char *title, u32 width, u32 height, bool centerd, bool resizable) {
	PlatformWindow result = {};
	int x = 0;
	int y = 0;
	if(centerd) {
		x = SDL_WINDOWPOS_CENTERED;
		y = SDL_WINDOWPOS_CENTERED;
		
	}
	u32 flags = 0;
	if(resizable) flags |= SDL_WINDOW_RESIZABLE;
	flags |= SDL_WINDOW_VULKAN;
	
	result.handle = SDL_CreateWindow(title, x, y, width, height, flags);
	
	SDL_SysWMinfo wmInfo;
	SDL_VERSION(&wmInfo.version);
	SDL_GetWindowWMInfo((SDL_Window *)result.handle, &wmInfo);
	result.platform_handle = wmInfo.info.win.window;
	return result;
}

void Platform::destroyWindow(PlatformWindow *window) {
	SDL_DestroyWindow((SDL_Window *)window->handle);
}

void Platform::processEvents(PlatformWindow *window, bool &requested_to_quit) {
	SDL_Event e;
	g_mouse_wheel = 0.0f;
	while(SDL_PollEvent(&e)) {
		// ImGuiImpl::processEvent(&e);
		switch(e.type) {
			case SDL_QUIT: {
				requested_to_quit = true;	
			} break;
			
			case SDL_WINDOWEVENT: {
				switch(e.window.event) {
					case SDL_WINDOWEVENT_SIZE_CHANGED: {
						s32 new_width = e.window.data1;
						s32 new_height = e.window.data2;
						// render_context.resizeBuffer(new_width, new_height);
					} break;
				}
			} break;
			
			case SDL_MOUSEWHEEL: {
				
				if(e.wheel.y > 0) g_mouse_wheel = 1.0f;
				else if(e.wheel.y < 0) g_mouse_wheel = -1.0f;
			} break;
			
			case SDL_TEXTINPUT: {
				if(on_text_input != 0) {
					on_text_input(e.text.text);
				}
			} break;
		}
	}
}

void Platform::getWindowSize(PlatformWindow *window, u32 &width, u32 &height) {
	int window_width, window_height;
	SDL_GetWindowSize((SDL_Window *)window->handle, &window_width, &window_height);
	width = (u32)window_width;
	height = (u32)window_height;
}

bool Platform::isWindowFullscreen(PlatformWindow *window) {
	u32 fullscreen_mode = SDL_WINDOW_FULLSCREEN_DESKTOP;
	return ContainsBits(SDL_GetWindowFlags((SDL_Window *)window->handle), fullscreen_mode);
}

void Platform::setWindowFullscreen(PlatformWindow *window, bool fullscreen) {
	u32 value = 0;
	if(!fullscreen) value = SDL_WINDOW_FULLSCREEN_DESKTOP;
	SDL_SetWindowFullscreen((SDL_Window *)window->handle, value);
}

void Platform::sleepMS(u32 ms) {
	SDL_Delay(ms);
}

void Platform::setWindowTitle(PlatformWindow *window, const char *title) {
	SDL_SetWindowTitle((SDL_Window *)window->handle, title);
}

void *Platform::loadLibrary(const char *name) {
	return SDL_LoadObject(name);	
}

void Platform::unloadLibrary(void *library) {
	SDL_UnloadObject(library);
}

void *Platform::loadFunction(void *library, const char *name) {
	return SDL_LoadFunction(library, name);
}

char *Platform::getExePath() {
	return SDL_GetBasePath();
}

u64 Platform::getPerformanceCounter() {
	return SDL_GetPerformanceCounter();
}

u64 Platform::getPerformanceFrequency() {
	return SDL_GetPerformanceFrequency();
}

bool Platform::getKeyDown(Key key) {
	u32 k = g_sdl_keys[(s32)key];
	SDL_Scancode scancode = SDL_GetScancodeFromKey(k); // NOTE(nathan): could store scancodes instead of keys if there is too much overhead here?
	return g_keyboard_state[scancode] != 0;
}

bool Platform::getMouseDown(MouseButton button) {
	s32 mouse_state = SDL_GetMouseState(0, 0);
	return mouse_state & SDL_BUTTON((s32)button+1);
}

void Platform::getMousePosition(s32 &x, s32 &y) {
	s32 mouse_x, mouse_y;
	SDL_GetMouseState(&mouse_x, &mouse_y);
	x = mouse_x;
	y = mouse_y;
}

void Platform::setMousePosition(s32 x, s32 y) {
	SDL_WarpMouseInWindow(SDL_GetMouseFocus(), x, y);
}

f32 Platform::getMouseWheel() {
	return g_mouse_wheel;
}

void Platform::setCursorVisible(bool visible) {
	SDL_ShowCursor(visible ? 1 : 0);
}

const char *Platform::getClipboardText() {
	return SDL_GetClipboardText();	
}

void Platform::setClipboardText(const char *text) {
	SDL_SetClipboardText(text);
}
