// OpenGL3 renderer backend for ImGui
#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include <glad/glad.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char         g_GlslVersionString[32] = "#version 330 core";
static GLuint       g_FontTexture = 0;
static GLuint       g_ShaderHandle = 0, g_VertHandle = 0, g_FragHandle = 0;
static int          g_AttribLocationTex = 0, g_AttribLocationProjMtx = 0;
static int          g_AttribLocationPosition = 0, g_AttribLocationUV = 1, g_AttribLocationColor = 2;
static GLuint       g_VboHandle = 0, g_ElementsHandle = 0, g_VaoHandle = 0;

static const char* ImGui_ImplOpenGL3_GetVertexShaderSrc()
{
	return "#version 330 core\n"
		   "layout (location = 0) in vec2 Position;\n"
		   "layout (location = 1) in vec2 UV;\n"
		   "layout (location = 2) in vec4 Color;\n"
		   "out vec2 Frag_UV;\n"
		   "out vec4 Frag_Color;\n"
		   "uniform mat4 ProjMtx;\n"
		   "void main()\n"
		   "{\n"
		   "    Frag_UV = UV;\n"
		   "    Frag_Color = Color;\n"
		   "    gl_Position = ProjMtx * vec4(Position.xy, 0.0, 1.0);\n"
		   "}\n";
}

static const char* ImGui_ImplOpenGL3_GetFragmentShaderSrc()
{
	return "#version 330 core\n"
		   "in vec2 Frag_UV;\n"
		   "in vec4 Frag_Color;\n"
		   "uniform sampler2D Texture;\n"
		   "out vec4 Out_Color;\n"
		   "void main()\n"
		   "{\n"
		   "    Out_Color = Frag_Color * texture(Texture, Frag_UV.st);\n"
		   "}\n";
}

static GLuint ImGui_ImplOpenGL3_CreateShader(const char* src, GLenum type)
{
	GLuint handle = glCreateShader(type);
	glShaderSource(handle, 1, &src, NULL);
	glCompileShader(handle);
	GLint status = 0;
	glGetShaderiv(handle, GL_COMPILE_STATUS, &status);
	if (!status)
	{
		char buf[512];
		glGetShaderInfoLog(handle, 512, NULL, buf);
		fprintf(stderr, "Shader compile error: %s\n", buf);
	}
	return handle;
}

bool ImGui_ImplOpenGL3_Init(const char* glsl_version)
{
	if (glsl_version && glsl_version[0])
		strncpy(g_GlslVersionString, glsl_version, sizeof(g_GlslVersionString)-1);

	// Create shader
	const char* vertex_src = ImGui_ImplOpenGL3_GetVertexShaderSrc();
	const char* fragment_src = ImGui_ImplOpenGL3_GetFragmentShaderSrc();
	g_VertHandle = ImGui_ImplOpenGL3_CreateShader(vertex_src, GL_VERTEX_SHADER);
	g_FragHandle = ImGui_ImplOpenGL3_CreateShader(fragment_src, GL_FRAGMENT_SHADER);
	g_ShaderHandle = glCreateProgram();
	glAttachShader(g_ShaderHandle, g_VertHandle);
	glAttachShader(g_ShaderHandle, g_FragHandle);
	glLinkProgram(g_ShaderHandle);

	g_AttribLocationTex = glGetUniformLocation(g_ShaderHandle, "Texture");
	g_AttribLocationProjMtx = glGetUniformLocation(g_ShaderHandle, "ProjMtx");

	// Create buffers
	glGenBuffers(1, &g_VboHandle);
	glGenBuffers(1, &g_ElementsHandle);
	glGenVertexArrays(1, &g_VaoHandle);
	glBindVertexArray(g_VaoHandle);
	glBindBuffer(GL_ARRAY_BUFFER, g_VboHandle);
	glEnableVertexAttribArray(g_AttribLocationPosition);
	glEnableVertexAttribArray(g_AttribLocationUV);
	glEnableVertexAttribArray(g_AttribLocationColor);
	// Note: IM_OFFSETOF macro may not be available; compute offsets manually
	glVertexAttribPointer(g_AttribLocationPosition, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)offsetof(ImDrawVert, pos));
	glVertexAttribPointer(g_AttribLocationUV, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)offsetof(ImDrawVert, uv));
	glVertexAttribPointer(g_AttribLocationColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid*)offsetof(ImDrawVert, col));
	glBindVertexArray(0);

	// Create fonts texture
	ImGuiIO& io = ImGui::GetIO();
	unsigned char* pixels;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
	glGenTextures(1, &g_FontTexture);
	glBindTexture(GL_TEXTURE_2D, g_FontTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	io.Fonts->SetTexID((ImTextureID)(intptr_t)g_FontTexture);

	// Restore state
	glBindTexture(GL_TEXTURE_2D, 0);

	return true;
}

void ImGui_ImplOpenGL3_Shutdown()
{
	if (g_VaoHandle) { glDeleteVertexArrays(1, &g_VaoHandle); g_VaoHandle = 0; }
	if (g_VboHandle) { glDeleteBuffers(1, &g_VboHandle); g_VboHandle = 0; }
	if (g_ElementsHandle) { glDeleteBuffers(1, &g_ElementsHandle); g_ElementsHandle = 0; }
	if (g_ShaderHandle) { glDeleteProgram(g_ShaderHandle); g_ShaderHandle = 0; }
	if (g_VertHandle) { glDeleteShader(g_VertHandle); g_VertHandle = 0; }
	if (g_FragHandle) { glDeleteShader(g_FragHandle); g_FragHandle = 0; }
	if (g_FontTexture) { glDeleteTextures(1, &g_FontTexture); ImGui::GetIO().Fonts->SetTexID(0); g_FontTexture = 0; }
}

void ImGui_ImplOpenGL3_NewFrame()
{
	// nothing to do here for now
}

void ImGui_ImplOpenGL3_RenderDrawData(ImDrawData* draw_data)
{
	if (!draw_data || draw_data->CmdListsCount == 0)
		return;

	// Backup GL state
	GLint last_program; glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
	GLint last_texture; glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
	GLint last_active_texture; glGetIntegerv(GL_ACTIVE_TEXTURE, &last_active_texture);
	GLint last_array_buffer; glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
	GLint last_element_array_buffer; glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &last_element_array_buffer);
	GLint last_vertex_array; glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);
	GLint last_blend_src_rgb; glGetIntegerv(GL_BLEND_SRC_RGB, &last_blend_src_rgb);
	GLint last_blend_dst_rgb; glGetIntegerv(GL_BLEND_DST_RGB, &last_blend_dst_rgb);
	GLint last_blend_src_alpha; glGetIntegerv(GL_BLEND_SRC_ALPHA, &last_blend_src_alpha);
	GLint last_blend_dst_alpha; glGetIntegerv(GL_BLEND_DST_ALPHA, &last_blend_dst_alpha);
	GLboolean last_enable_blend = glIsEnabled(GL_BLEND);
	GLboolean last_enable_cull_face = glIsEnabled(GL_CULL_FACE);
	GLboolean last_enable_depth_test = glIsEnabled(GL_DEPTH_TEST);
	GLboolean last_enable_scissor_test = glIsEnabled(GL_SCISSOR_TEST);

	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_SCISSOR_TEST);

	// Setup viewport
	int fb_width = (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
	int fb_height = (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
	if (fb_width == 0 || fb_height == 0)
		return;
	glViewport(0, 0, fb_width, fb_height);

	// Setup orthographic projection matrix
	const float L = draw_data->DisplayPos.x;
	const float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
	const float T = draw_data->DisplayPos.y;
	const float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
	float ortho_projection[4][4] = {
		{ 2.0f/(R-L), 0.0f,         0.0f, 0.0f },
		{ 0.0f,       2.0f/(T-B),   0.0f, 0.0f },
		{ 0.0f,       0.0f,        -1.0f, 0.0f },
		{ (R+L)/(L-R), (T+B)/(B-T), 0.0f, 1.0f },
	};

	glUseProgram(g_ShaderHandle);
	glUniform1i(g_AttribLocationTex, 0);
	glUniformMatrix4fv(g_AttribLocationProjMtx, 1, GL_FALSE, &ortho_projection[0][0]);
	glBindVertexArray(g_VaoHandle);

	// Upload data
	size_t total_vtx_count = 0;
	size_t total_idx_count = 0;
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		total_vtx_count += draw_data->CmdLists[n]->VtxBuffer.Size;
		total_idx_count += draw_data->CmdLists[n]->IdxBuffer.Size;
	}

	glBindBuffer(GL_ARRAY_BUFFER, g_VboHandle);
	glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(total_vtx_count * sizeof(ImDrawVert)), NULL, GL_STREAM_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ElementsHandle);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(total_idx_count * sizeof(ImDrawIdx)), NULL, GL_STREAM_DRAW);

	// Copy and draw
	size_t vtx_offset = 0;
	size_t idx_offset = 0;
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)(vtx_offset * sizeof(ImDrawVert)), (GLsizeiptr)(cmd_list->VtxBuffer.Size * sizeof(ImDrawVert)), (const GLvoid*)cmd_list->VtxBuffer.Data);
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, (GLintptr)(idx_offset * sizeof(ImDrawIdx)), (GLsizeiptr)(cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx)), (const GLvoid*)cmd_list->IdxBuffer.Data);

		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
		{
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
			if (pcmd->UserCallback)
			{
				pcmd->UserCallback(cmd_list, pcmd);
			}
			else
			{
				ImTextureID tex_ref = pcmd->GetTexID();
				GLuint tex_id = (GLuint)(intptr_t)tex_ref;
				glBindTexture(GL_TEXTURE_2D, tex_id);
				glScissor((int)pcmd->ClipRect.x, (int)(fb_height - pcmd->ClipRect.w),
						  (int)(pcmd->ClipRect.z - pcmd->ClipRect.x), (int)(pcmd->ClipRect.w - pcmd->ClipRect.y));
				// Note: ImDrawCmd uses VtxOffset/IdxOffset fields; we accumulate them when uploading
				glDrawElementsBaseVertex(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, (void*)(idx_offset * sizeof(ImDrawIdx) + pcmd->IdxOffset * sizeof(ImDrawIdx)), (GLint)vtx_offset + (GLint)pcmd->VtxOffset);
			}
		}

		vtx_offset += cmd_list->VtxBuffer.Size;
		idx_offset += cmd_list->IdxBuffer.Size;
	}

	// Restore modified GL state
	glUseProgram((GLuint)last_program);
	glBindTexture(GL_TEXTURE_2D, (GLuint)last_texture);
	glActiveTexture((GLuint)last_active_texture);
	glBindBuffer(GL_ARRAY_BUFFER, (GLuint)last_array_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, (GLuint)last_element_array_buffer);
	glBindVertexArray((GLuint)last_vertex_array);
	if (last_enable_blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
	if (last_enable_cull_face) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
	if (last_enable_depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
	if (last_enable_scissor_test) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
}
