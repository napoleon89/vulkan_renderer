namespace ImGuiImpl {
	static ShaderConstant im_matrix;
	static Shader im_shader;
	static ShaderLayout im_layout;
	static RasterState im_raster_state;
	static BlendState im_blend_state;
	static DepthStencilState im_depth_stencil_state;
	static Texture2D im_font_texture;
	static Sampler im_font_sampler;

	struct VERTEX_CONSTANT_BUFFER
	{
		float        mvp[4][4];
	};
	
	static void render(RenderContext *ctx) {
		ImDrawData *draw_data = ImGui::GetDrawData();
		int index_count = 0;
		int vertex_count = 0;
		for (int n = 0; n < draw_data->CmdListsCount; n++)
		{
			const ImDrawList* cmd_list = draw_data->CmdLists[n];
			
			vertex_count += cmd_list->VtxBuffer.Size;
			index_count += cmd_list->IdxBuffer.Size;
		}
		
		ImDrawVert *vertices = (ImDrawVert *)malloc(vertex_count * sizeof(ImDrawVert));
		ImDrawIdx *indices = (ImDrawIdx *)malloc(index_count * sizeof(ImDrawIdx));
		
		ImDrawVert* vtx_dst = (ImDrawVert*)vertices;
		ImDrawIdx* idx_dst = (ImDrawIdx*)indices;
		for (int n = 0; n < draw_data->CmdListsCount; n++) {
			const ImDrawList* cmd_list = draw_data->CmdLists[n];
			memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
			memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
			vtx_dst += cmd_list->VtxBuffer.Size;
			idx_dst += cmd_list->IdxBuffer.Size;
		}
		
		VertexBuffer vb = ctx->createVertexBuffer(vertices, sizeof(ImDrawVert), vertex_count);
		VertexBuffer ib = ctx->createVertexBuffer(indices, sizeof(ImDrawIdx), index_count, RenderContext::BufferType::Index);

		PlatformRenderState *render_state = ctx->saveRenderState();

		ctx->bindShader(&im_shader);

		// Setup orthographic projection matrix into our constant buffer
		{
			
			float L = 0.0f;
			float R = ImGui::GetIO().DisplaySize.x;
			float B = ImGui::GetIO().DisplaySize.y;
			float T = 0.0f;
			float mvp[4][4] =
			{
				{ 2.0f/(R-L),   0.0f,           0.0f,       0.0f },
				{ 0.0f,         2.0f/(T-B),     0.0f,       0.0f },
				{ 0.0f,         0.0f,           0.5f,       0.0f },
				{ (R+L)/(L-R),  (T+B)/(B-T),    0.5f,       1.0f },
			};
			
			ctx->updateShaderConstant(&im_matrix, mvp);
			ctx->bindShaderConstant(&im_matrix, 0, -1);
		}
		
		ctx->setViewport(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y, 0.0f, 1.0f);

		// Bind shader and vertex buffers
		unsigned int stride = sizeof(ImDrawVert);
		unsigned int offset = 0;
		
		ctx->bindShaderLayout(&im_layout);
		ctx->bindVertexBuffer(&vb, 0);
		ctx->bindIndexBuffer(&ib, sizeof(ImDrawIdx) == 2 ? RenderContext::Format::u16 : RenderContext::Format::u32);
		
		
		
		ctx->bindSampler(&im_font_sampler, 0);
		
		// Setup render state
		const float blend_factor[4] = { 0.f, 0.f, 0.f, 0.f };
		ctx->bindBlendState(&im_blend_state, blend_factor, 0xffffffff);
		
		ctx->bindDepthStencilState(&im_depth_stencil_state);
		ctx->bindRasterState(&im_raster_state);

		// Render command lists
		int vtx_offset = 0;
		int idx_offset = 0;
		for (int n = 0; n < draw_data->CmdListsCount; n++)
		{
			const ImDrawList* cmd_list = draw_data->CmdLists[n];
			for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
			{
				const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
				if (pcmd->UserCallback)
				{
					pcmd->UserCallback(cmd_list, pcmd);
				}
				else
				{
					Texture2D *tex = (Texture2D *)pcmd->TextureId;
					if(tex != 0)
						ctx->bindTexture2D(tex, 0);
					
					ctx->setClipRect(pcmd->ClipRect.x, pcmd->ClipRect.y, pcmd->ClipRect.z, pcmd->ClipRect.w);
					// VertexBuffer vb = ctx->createVertexBuffer(cmd_list->VtxBuffer.Data, sizeof(ImDrawVert), cmd_list->VtxBuffer.Size);
					// VertexBuffer ib = ctx->createVertexBuffer(cmd_list->IdxBuffer.Data, sizeof(ImDrawIdx), cmd_list->IdxBuffer.Size, RenderContext::BufferType::Index);
					
					
					// ctx->bindVertexBuffer(&vb, 0);
					// ctx->bindIndexBuffer(&ib, sizeof(ImDrawIdx) == 2 ? RenderContext::Format::u16 : RenderContext::Format::u32);
					ctx->sendDrawIndexed(RenderContext::Topology::TriangleList, pcmd->ElemCount, vtx_offset, idx_offset);
					
				}
				idx_offset += pcmd->ElemCount;
			}
			vtx_offset += cmd_list->VtxBuffer.Size;
		}
		
		free(vertices);
		free(indices);
		
		ctx->destroyVertexBuffer(&vb);
		ctx->destroyVertexBuffer(&ib);
		
		ctx->reloadRenderState(render_state);
		ctx->destroyRenderState(render_state);
	}
	
	static void createFontsTexture(RenderContext *ctx) {
		 // Build texture atlas
		ImGuiIO& io = ImGui::GetIO();
		unsigned char* pixels;
		int width, height;
		io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
		
		im_font_texture = ctx->createTexture2D(pixels, width, height, RenderContext::Format::u32_unorm);
		im_font_sampler = ctx->createSampler();
		io.Fonts->TexID = &im_font_texture;
	}
	
	static void invalidateDeviceObjects() {
		
	}
	
	static void createDeviceObjects(Platform *platform, RenderContext *ctx) {
		if (im_font_sampler.sampler)
			invalidateDeviceObjects();

		// By using D3DCompile() from <d3dcompiler.h> / d3dcompiler.lib, we introduce a dependency to a given version of d3dcompiler_XX.dll (see D3DCOMPILER_DLL_A)
		// If you would like to use this DX11 sample code but remove this dependency you can: 
		//  1) compile once, save the compiled shader blobs into a file or source code and pass them to CreateVertexShader()/CreatePixelShader() [preferred solution]
		//  2) use code to detect any version of the DLL and grab a pointer to D3DCompile from the DLL. 
		// See https://github.com/ocornut/imgui/pull/638 for sources and details.

		// Create the vertex shader
		im_shader = ctx->createShader(platform, "imgui");

		RenderContext::LayoutElement layout_elements[] = {
			{"POSITION", RenderContext::Format::Vec2},
			{"TEXCOORD", RenderContext::Format::Vec2},
			{"COLOR", RenderContext::Format::u32_unorm},
		};
		
		im_layout = ctx->createShaderLayout(&layout_elements[0], ArrayCount(layout_elements), &im_shader);

		im_matrix = ctx->createShaderConstant(sizeof(VERTEX_CONSTANT_BUFFER));


		im_blend_state = ctx->createBlendState();
		
		im_raster_state = ctx->createRasterState(true, true);

		// Create depth-stencil State
		im_depth_stencil_state = ctx->createDepthStencilState();

		createFontsTexture(ctx);
	}
	
	
	static void shutdown() {
		invalidateDeviceObjects();
		ImGui::Shutdown();
	}
	
	static const char *GetClipboardText(void *data) {
		Platform *platform = (Platform *)data;
		return platform->getClipboardText();
	}
	
	static void setClipboardText(void *data, const char *text) {
		Platform *platform = (Platform *)data;
		platform->setClipboardText(text);
	}
	
	static void onTextInput(const char *text) {
		ImGuiIO& io = ImGui::GetIO();
		io.AddInputCharactersUTF8(text);	
	}
	
	static void init(Platform *platform, PlatformWindow *window) {
		ImGuiIO& io = ImGui::GetIO();
		io.KeyMap[ImGuiKey_Tab] = (int)Key::Tab;                     // Keyboard mapping. ImGui will use those indices to peek into the io.KeyDown[] array.
		io.KeyMap[ImGuiKey_LeftArrow] = (int)Key::Left;
		io.KeyMap[ImGuiKey_RightArrow] = (int)Key::Right;
		io.KeyMap[ImGuiKey_UpArrow] = (int)Key::Up;
		io.KeyMap[ImGuiKey_DownArrow] = (int)Key::Down;
		io.KeyMap[ImGuiKey_PageUp] = (int)Key::PageUp;
		io.KeyMap[ImGuiKey_PageDown] = (int)Key::PageDown;
		io.KeyMap[ImGuiKey_Home] = (int)Key::Home;
		io.KeyMap[ImGuiKey_End] = (int)Key::End;
		io.KeyMap[ImGuiKey_Delete] = (int)Key::Delete;
		io.KeyMap[ImGuiKey_Backspace] = (int)Key::Backspace;
		io.KeyMap[ImGuiKey_Enter] = (int)Key::Enter;
		io.KeyMap[ImGuiKey_Escape] = (int)Key::Escape;
		io.KeyMap[ImGuiKey_A] = (int)Key::A;
		io.KeyMap[ImGuiKey_C] = (int)Key::C;
		io.KeyMap[ImGuiKey_V] = (int)Key::V;
		io.KeyMap[ImGuiKey_X] = (int)Key::X;
		io.KeyMap[ImGuiKey_Y] = (int)Key::Y;
		io.KeyMap[ImGuiKey_Z] = (int)Key::Z;
		
		io.ImeWindowHandle = window->platform_handle;
		
		io.SetClipboardTextFn = setClipboardText;
		io.GetClipboardTextFn = GetClipboardText;
		io.ClipboardUserData = platform;
		
		io.MouseDrawCursor = true;
		platform->on_text_input = onTextInput;
	}
	
	static void newFrame(Platform *platform, PlatformWindow *window, RenderContext *ctx, float delta) {
		if (!im_font_sampler.sampler) createDeviceObjects(platform, ctx);

		ImGuiIO& io = ImGui::GetIO();

		// Setup display size (every frame to accommodate for window resizing)
		u32 w, h;
		int display_w, display_h;
		platform->getWindowSize(window, w, h);
		display_w = w;
		display_h = h;
		io.DisplaySize = ImVec2((float)w, (float)h);
		io.DisplayFramebufferScale = ImVec2(w > 0 ? ((float)display_w / w) : 0, h > 0 ? ((float)display_h / h) : 0);

		io.DeltaTime = delta;

		for(int i = 0; i < (int)Key::KeyCount; i++) {
			io.KeysDown[i] = platform->getKeyDown((Key)i);
		}
		
		io.KeyShift = platform->getKeyDown(Key::LShift) || platform->getKeyDown(Key::RShift);
		io.KeyCtrl = platform->getKeyDown(Key::LCtrl) || platform->getKeyDown(Key::RCtrl);
		io.KeyAlt = platform->getKeyDown(Key::LAlt) || platform->getKeyDown(Key::RAlt);
		io.KeySuper = platform->getKeyDown(Key::LGui) || platform->getKeyDown(Key::RGui);

		s32 mouse_x, mouse_y;
		platform->getMousePosition(mouse_x, mouse_y);
		io.MousePos = ImVec2(mouse_x, mouse_y);


		for(int i = 0; i < 3; i++) io.MouseDown[i] = platform->getMouseDown((MouseButton)i);

		io.MouseWheel = platform->getMouseWheel();

		// Hide OS mouse cursor if ImGui is drawing it
		platform->setCursorVisible(io.MouseDrawCursor);

		// Start the frame
		ImGui::NewFrame();
	}
}