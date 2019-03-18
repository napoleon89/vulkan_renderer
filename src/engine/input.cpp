struct ButtonState {
	bool is_down;
	bool was_down;	
};

struct MouseState {
	union {
		ButtonState buttons[3];
		struct {
			ButtonState left;	
			ButtonState middle;	
			ButtonState right;	
		};
	};
	s32 x, y;
};

class InputManager {
	private:
		ButtonState buttons[2][Key::KeyCount];
		ButtonState (*old_buttons)[Key::KeyCount];
		ButtonState (*new_buttons)[Key::KeyCount];
		MouseState mouse_states[2];
		MouseState *old_mouse;
		MouseState *new_mouse;
		PlatformWindow *window;
		
		void processKey(Platform *platform, ButtonState *old_input, ButtonState *new_input, Key key) {
			new_input->is_down = platform->getKeyDown(key);
			new_input->was_down = old_input->is_down;
		}
		
	public:
		
		InputManager(PlatformWindow *w) {
			window = w;
			old_mouse = &mouse_states[0];
			new_mouse = &mouse_states[1];
			old_buttons = &buttons[0];
			new_buttons = &buttons[1];
		}
		
		
		bool isKeyDown(Key key) {
			return (*new_buttons)[(int)key].is_down;
		}
		
		bool isKeyDownOnce(Key key) {
			return ((*new_buttons)[(int)key].is_down && !(*new_buttons)[(int)key].was_down);
		}
		
		// relative to window
		Vec2i getMousePosition() {
			return Vec2i(new_mouse->x, new_mouse->y);
		}
		
		Vec2i getMouseDelta() {
			return getMousePosition() - Vec2i(old_mouse->x, old_mouse->y);
		}
		
		bool isMouseButtonDown(MouseButton button) {
			return new_mouse->buttons[(s32)button].is_down;
		}
		
		bool isMouseButtonDownOnce(MouseButton button) {
			return new_mouse->buttons[(s32)button].is_down && !new_mouse->buttons[(s32)button].was_down;
		}
		
		
		// relative to window
		void setMousePosition(Platform *platform, s32 x, s32 y) {
			platform->setMousePosition(x, y);
		}
		
		void processKeys(Platform *platform) {
			
			for(int i = 0; i < (int)Key::KeyCount; i++) {
				processKey(platform, (old_buttons[0]+i), (new_buttons[0]+i), (Key)i);
			}
			
			platform->getMousePosition(new_mouse->x, new_mouse->y);
			
			// if(!ImGui::IsMouseHoveringAnyWindow() && !ImGui::IsAnyItemActive()) {
				for(u32 i = 0; i < 3; i++)
				{
					new_mouse->buttons[i].is_down = platform->getMouseDown((MouseButton)i);
					new_mouse->buttons[i].was_down = old_mouse->buttons[i].is_down;
				}
			// }
		}
		
		void endFrame() {
			Swap(old_buttons, new_buttons);
			Swap(old_mouse, new_mouse);
		}
		
		f32 getScrollAxis(Platform *platform) { return platform->getMouseWheel(); }
};