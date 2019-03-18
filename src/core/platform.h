#ifndef PLATFORM_H
#define PLATFORM_H

#include <engine/std.h>

struct FileData {
	char *contents;
	u64 size;	
};

struct FileTime {
	u32 low_date_time;
	u32 high_date_time;	
};

struct PlatformWindow {
	void *handle;
	void *platform_handle;
	bool open;
};

enum class MouseButton {
	Left,
	Middle,
	Right,	
};

enum class Key {
	Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
	A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
	NumPad0, NumPad1, NumPad2, NumPad3, NumPad4, NumPad5, NumPad6, NumPad7, NumPad8, NumPad9,
	F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12, F13, F14, F15, F16, F17, F18, F19, F20, F21, F22, F23, F24,
	Left, Right, Up, Down, RCtrl, LCtrl, LShift, RShift, Escape, Enter, Tab, LAlt, RAlt,
	PageUp, PageDown, Home, End, Delete, Backspace, LGui, RGui,
	KeyCount
};

typedef void (TextInputFunc)(const char *);

struct Platform {
	TextInputFunc *on_text_input;
	
	virtual bool init();
	virtual void uninit();

	virtual void copyFile(char *a, char *b);
	virtual FileTime getLastWriteTime(char *file);
	virtual u32 compareFileTime(FileTime *a, FileTime *b);
	
	virtual void getDirectoryContents();
	virtual FileData readEntireFile(const char *filename);
	virtual void writeStructureToFile(const char *filename, void *structure, s32 size);
	virtual void *openFileForWriting(const char *filename);
	virtual void closeOpenFile(void *file);
	virtual void writeToFile(void *file, void *structure, s32 size);
	
	virtual void *alloc(u64 size);
	virtual void free(void *data);
	
	// NOTE(nathan): an error message box
	virtual void error(char *err);
		
	virtual PlatformWindow createWindow(const char *title, u32 width, u32 height, bool centerd, bool resizable);
	virtual void destroyWindow(PlatformWindow *window);
	virtual void processEvents(PlatformWindow *window, bool &requested_to_quit);
	virtual void getWindowSize(PlatformWindow *window, u32 &x, u32 &y);
	virtual bool isWindowFullscreen(PlatformWindow *window);
	virtual void setWindowFullscreen(PlatformWindow *window, bool fullscreen);
	virtual void setWindowTitle(PlatformWindow *window, const char *title);
	
	virtual void sleepMS(u32 ms);
	
	virtual void *loadLibrary(const char *name);
	virtual void unloadLibrary(void *library);
	virtual void *loadFunction(void *library, const char *name);
	
	virtual char *getExePath();
	
	virtual u64 getPerformanceCounter();
	virtual u64 getPerformanceFrequency();
	
	// virtual u32 getControllerCount();
	virtual bool getKeyDown(Key key);
	virtual bool getMouseDown(MouseButton button);
	virtual void getMousePosition(s32 &x, s32 &y); // NOTE(nathan): relative to window
	virtual void setMousePosition(s32 x, s32 y); // NOTE(nathan): relative to window
	virtual f32 getMouseWheel();
	virtual void setCursorVisible(bool visible);
	
	virtual const char *getClipboardText();
	virtual void setClipboardText(const char *text);
};

#endif // PLATFORM_H