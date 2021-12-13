/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "common/GL/Context.h"
#include "common/GL/StreamBuffer.h"
#include "GS/Renderers/Common/GSDevice.h"
#include "GSTextureOGL.h"
#include "GSUniformBufferOGL.h"
#include "GSShaderOGL.h"
#include "GLState.h"
#include "GS/GS.h"

#ifdef ENABLE_OGL_DEBUG_MEM_BW
extern u64 g_real_texture_upload_byte;
extern u64 g_vertex_upload_byte;
#endif

class GSDepthStencilOGL
{
	bool m_depth_enable;
	GLenum m_depth_func;
	bool m_depth_mask;
	// Note front face and back might be split but it seems they have same parameter configuration
	bool m_stencil_enable;
	GLenum m_stencil_func;
	GLenum m_stencil_spass_dpass_op;

public:
	GSDepthStencilOGL()
		: m_depth_enable(false)
		, m_depth_func(GL_ALWAYS)
		, m_depth_mask(0)
		, m_stencil_enable(false)
		, m_stencil_func(0)
		, m_stencil_spass_dpass_op(GL_KEEP)
	{
	}

	void EnableDepth() { m_depth_enable = true; }
	void EnableStencil() { m_stencil_enable = true; }

	void SetDepth(GLenum func, bool mask)
	{
		m_depth_func = func;
		m_depth_mask = mask;
	}
	void SetStencil(GLenum func, GLenum pass)
	{
		m_stencil_func = func;
		m_stencil_spass_dpass_op = pass;
	}

	void SetupDepth()
	{
		if (GLState::depth != m_depth_enable)
		{
			GLState::depth = m_depth_enable;
			if (m_depth_enable)
				glEnable(GL_DEPTH_TEST);
			else
				glDisable(GL_DEPTH_TEST);
		}

		if (m_depth_enable)
		{
			if (GLState::depth_func != m_depth_func)
			{
				GLState::depth_func = m_depth_func;
				glDepthFunc(m_depth_func);
			}
			if (GLState::depth_mask != m_depth_mask)
			{
				GLState::depth_mask = m_depth_mask;
				glDepthMask((GLboolean)m_depth_mask);
			}
		}
	}

	void SetupStencil()
	{
		if (GLState::stencil != m_stencil_enable)
		{
			GLState::stencil = m_stencil_enable;
			if (m_stencil_enable)
				glEnable(GL_STENCIL_TEST);
			else
				glDisable(GL_STENCIL_TEST);
		}

		if (m_stencil_enable)
		{
			// Note: here the mask control which bitplane is considered by the operation
			if (GLState::stencil_func != m_stencil_func)
			{
				GLState::stencil_func = m_stencil_func;
				glStencilFunc(m_stencil_func, 1, 1);
			}
			if (GLState::stencil_pass != m_stencil_spass_dpass_op)
			{
				GLState::stencil_pass = m_stencil_spass_dpass_op;
				glStencilOp(GL_KEEP, GL_KEEP, m_stencil_spass_dpass_op);
			}
		}
	}

	bool IsMaskEnable() { return m_depth_mask != GL_FALSE; }
};

class GSDeviceOGL final : public GSDevice
{
public:
	struct alignas(32) VSConstantBuffer
	{
		GSVector4 Vertex_Scale_Offset;

		GSVector4 Texture_Scale_Offset;

		GSVector2 PointSize;
		GSVector2i MaxDepth;

		VSConstantBuffer()
		{
			Vertex_Scale_Offset  = GSVector4::zero();
			Texture_Scale_Offset = GSVector4::zero();
			PointSize            = GSVector2(0);
			MaxDepth             = GSVector2i(0);
		}

		__forceinline bool Update(const VSConstantBuffer* cb)
		{
			GSVector4i* a = (GSVector4i*)this;
			GSVector4i* b = (GSVector4i*)cb;

			if (!((a[0] == b[0]) & (a[1] == b[1]) & (a[2] == b[2])).alltrue())
			{
				a[0] = b[0];
				a[1] = b[1];
				a[2] = b[2];

				return true;
			}

			return false;
		}
	};

	struct VSSelector
	{
		union
		{
			struct
			{
				u32 int_fst : 1;
				u32 _free : 31;
			};

			u32 key;
		};

		operator u32() const { return key; }

		VSSelector()
			: key(0)
		{
		}
		VSSelector(u32 k)
			: key(k)
		{
		}
	};

	struct GSSelector
	{
		union
		{
			struct
			{
				u32 sprite : 1;
				u32 point  : 1;
				u32 line   : 1;

				u32 _free : 29;
			};

			u32 key;
		};

		operator u32() const { return key; }

		GSSelector()
			: key(0)
		{
		}
		GSSelector(u32 k)
			: key(k)
		{
		}
	};

	struct alignas(32) PSConstantBuffer
	{
		GSVector4 FogColor_AREF;
		GSVector4 WH;
		GSVector4 TA_MaxDepth_Af;
		GSVector4i MskFix;
		GSVector4i FbMask;

		GSVector4 HalfTexel;
		GSVector4 MinMax;
		GSVector4 TC_OH;

		GSVector4 DitherMatrix[4];

		PSConstantBuffer()
		{
			FogColor_AREF  = GSVector4::zero();
			HalfTexel      = GSVector4::zero();
			WH             = GSVector4::zero();
			TA_MaxDepth_Af = GSVector4::zero();
			MinMax         = GSVector4::zero();
			MskFix         = GSVector4i::zero();
			TC_OH          = GSVector4::zero();
			FbMask         = GSVector4i::zero();

			DitherMatrix[0] = GSVector4::zero();
			DitherMatrix[1] = GSVector4::zero();
			DitherMatrix[2] = GSVector4::zero();
			DitherMatrix[3] = GSVector4::zero();
		}

		__forceinline bool Update(const PSConstantBuffer* cb)
		{
			GSVector4i* a = (GSVector4i*)this;
			GSVector4i* b = (GSVector4i*)cb;

			// if WH matches both HalfTexel and TC_OH_TS do too
			if (!((a[0] == b[0]) & (a[1] == b[1]) & (a[2] == b[2]) & (a[3] == b[3]) & (a[4] == b[4]) & (a[6] == b[6])
				& (a[8] == b[8]) & (a[9] == b[9]) & (a[10] == b[10]) & (a[11] == b[11])).alltrue())
			{
				// Note previous check uses SSE already, a plain copy will be faster than any memcpy
				a[0] = b[0];
				a[1] = b[1];
				a[2] = b[2];
				a[3] = b[3];
				a[4] = b[4];
				a[5] = b[5];
				a[6] = b[6];

				a[8] = b[8];
				a[9] = b[9];
				a[10] = b[10];
				a[11] = b[11];

				return true;
			}

			return false;
		}
	};

	using PSSelector = GSHWDrawConfig::PSSelector;
	using PSSamplerSelector = GSHWDrawConfig::SamplerSelector;
	using OMDepthStencilSelector = GSHWDrawConfig::DepthStencilSelector;
	using OMColorMaskSelector = GSHWDrawConfig::ColorMaskSelector;

	struct alignas(32) MiscConstantBuffer
	{
		GSVector4i ScalingFactor;
		GSVector4i ChannelShuffle;
		GSVector4i EMOD_AC;

		MiscConstantBuffer() { memset(this, 0, sizeof(*this)); }
	};

	static int m_shader_inst;
	static int m_shader_reg;

private:
	std::unique_ptr<GL::Context> m_gl_context;
	int m_mipmap;
	TriFiltering m_filter;

	static bool m_debug_gl_call;
	static FILE* m_debug_gl_file;

	bool m_disable_hw_gl_draw;

	// Place holder for the GLSL shader code (to avoid useless reload)
	std::string m_shader_common_header;
	std::string m_shader_tfx_vgs;
	std::string m_shader_tfx_fs;

	GLuint m_fbo; // frame buffer container
	GLuint m_fbo_read; // frame buffer container only for reading

	std::unique_ptr<GL::StreamBuffer> m_vertex_stream_buffer;
	std::unique_ptr<GL::StreamBuffer> m_index_stream_buffer;
	GLuint m_vertex_array_object = 0;
	GLenum m_draw_topology = 0;

	std::unique_ptr<GL::StreamBuffer> m_vertex_uniform_stream_buffer;
	std::unique_ptr<GL::StreamBuffer> m_fragment_uniform_stream_buffer;
	GLint m_uniform_buffer_alignment = 0;

	struct
	{
		GLuint ps[2]; // program object
		GSUniformBufferOGL* cb; // uniform buffer object
	} m_merge_obj;

	struct
	{
		GLuint ps[4]; // program object
		GSUniformBufferOGL* cb; // uniform buffer object
	} m_interlace;

	struct
	{
		GLuint vs; // program object
		GLuint ps[(int)ShaderConvert::Count]; // program object
		GLuint ln; // sampler object
		GLuint pt; // sampler object
		GSDepthStencilOGL* dss;
		GSDepthStencilOGL* dss_write;
		GSUniformBufferOGL* cb;
	} m_convert;

	struct
	{
		GLuint ps;
		GSUniformBufferOGL* cb;
	} m_fxaa;

	struct
	{
		GLuint ps;
		GSUniformBufferOGL* cb;
	} m_shaderfx;

	struct
	{
		GSDepthStencilOGL* dss;
		GSTexture* t;
	} m_date;

	struct
	{
		GLuint ps;
	} m_shadeboost;

	struct
	{
		u16 last_query;
		GLuint timer_query[1 << 16];

		GLuint timer() { return timer_query[last_query]; }
	} m_profiler;

	GLuint m_vs[1 << 1];
	GLuint m_gs[1 << 3];
	GLuint m_ps_ss[1 << 7];
	GSDepthStencilOGL* m_om_dss[1 << 5];
	std::unordered_map<u64, GLuint> m_ps;
	GLuint m_apitrace;

	GLuint m_palette_ss;

	VSConstantBuffer m_vs_cb_cache;
	PSConstantBuffer m_ps_cb_cache;
	MiscConstantBuffer m_misc_cb_cache;

	std::unique_ptr<GSTexture> m_font;
	AlignedBuffer<u8, 32> m_download_buffer;

	GSTexture* CreateSurface(GSTexture::Type type, int w, int h, GSTexture::Format format) final;

	void DoMerge(GSTexture* sTex[3], GSVector4* sRect, GSTexture* dTex, GSVector4* dRect, const GSRegPMODE& PMODE, const GSRegEXTBUF& EXTBUF, const GSVector4& c) final;
	void DoInterlace(GSTexture* sTex, GSTexture* dTex, int shader, bool linear, float yoffset = 0) final;
	void DoFXAA(GSTexture* sTex, GSTexture* dTex) final;
	void DoShadeBoost(GSTexture* sTex, GSTexture* dTex) final;
	void DoExternalFX(GSTexture* sTex, GSTexture* dTex) final;
	void RenderOsd(GSTexture* dt) final;

	void OMAttachRt(GSTextureOGL* rt = NULL);
	void OMAttachDs(GSTextureOGL* ds = NULL);
	void OMSetFBO(GLuint fbo);

	u16 ConvertBlendEnum(u16 generic) final;

	void DrawStretchRect(const GSVector4& sRect, const GSVector4& dRect, const GSVector2i& ds);

public:
	GSShaderOGL* m_shader;

	GSDeviceOGL();
	virtual ~GSDeviceOGL();

	void GenerateProfilerData();

	// Used by OpenGL, so the same calling convention is required.
	static void APIENTRY DebugOutputToFile(GLenum gl_source, GLenum gl_type, GLuint id, GLenum gl_severity, GLsizei gl_length, const GLchar* gl_message, const void* userParam);

	bool Create(const WindowInfo& wi) override;
	bool Reset(int w, int h) override;
	void Flip() override;
	void SetVSync(int vsync) override;

	void DrawPrimitive();
	void DrawIndexedPrimitive();
	void DrawIndexedPrimitive(int offset, int count);

	void ClearRenderTarget(GSTexture* t, const GSVector4& c) final;
	void ClearRenderTarget(GSTexture* t, u32 c) final;
	void ClearDepth(GSTexture* t) final;
	void ClearStencil(GSTexture* t, u8 c) final;

	void InitPrimDateTexture(GSTexture* rt, const GSVector4i& area);
	void RecycleDateTexture();

	bool DownloadTexture(GSTexture* src, const GSVector4i& rect, GSTexture::GSMap& out_map) final;

	void CopyRect(GSTexture* sTex, GSTexture* dTex, const GSVector4i& r) final;

	// BlitRect *does* mess with GL state, be sure to re-bind.
	void BlitRect(GSTexture* sTex, const GSVector4i& r, const GSVector2i& dsize, bool at_origin, bool linear);

	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, ShaderConvert shader = ShaderConvert::COPY, bool linear = true) final;
	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, GLuint ps, bool linear = true);
	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, bool red, bool green, bool blue, bool alpha) final;
	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, GLuint ps, int bs, OMColorMaskSelector cms, bool linear = true);

	void RenderHW(GSHWDrawConfig& config) final;
	void SendHWDraw(const GSHWDrawConfig& config);

	void SetupDATE(GSTexture* rt, GSTexture* ds, const GSVertexPT1* vertices, bool datm);

	void IASetPrimitiveTopology(GLenum topology);
	void IASetVertexBuffer(const void* vertices, size_t count);
	void IASetIndexBuffer(const void* index, size_t count);

	void PSSetShaderResource(int i, GSTexture* sr);
	void PSSetShaderResources(GSTexture* sr0, GSTexture* sr1);
	void PSSetSamplerState(GLuint ss);

	void OMSetDepthStencilState(GSDepthStencilOGL* dss);
	void OMSetBlendState(u8 blend_index = 0, u8 blend_factor = 0, bool is_blend_constant = false, bool accumulation_blend = false, bool blend_mix = false);
	void OMSetRenderTargets(GSTexture* rt, GSTexture* ds, const GSVector4i* scissor = NULL);
	void OMSetColorMaskState(OMColorMaskSelector sel = OMColorMaskSelector());

	bool HasColorSparse() final { return GLLoader::found_compatible_GL_ARB_sparse_texture2; }
	bool HasDepthSparse() final { return GLLoader::found_compatible_sparse_depth; }

	bool CreateTextureFX();
	GLuint CompileVS(VSSelector sel);
	GLuint CompileGS(GSSelector sel);
	GLuint CompilePS(PSSelector sel);
	GLuint CreateSampler(PSSamplerSelector sel);
	GSDepthStencilOGL* CreateDepthStencil(OMDepthStencilSelector dssel);

	void SelfShaderTestPrint(const std::string& test, int& nb_shader);
	void SelfShaderTestRun(const std::string& dir, const std::string& file, const PSSelector& sel, int& nb_shader);
	void SelfShaderTest();

	void SetupPipeline(const VSSelector& vsel, const GSSelector& gsel, const PSSelector& psel);
	void SetupCB(const VSConstantBuffer* vs_cb, const PSConstantBuffer* ps_cb);
	void SetupCBMisc(const GSVector4i& channel);
	void SetupSampler(PSSamplerSelector ssel);
	void SetupOM(OMDepthStencilSelector dssel);
	GLuint GetSamplerID(PSSamplerSelector ssel);
	GLuint GetPaletteSamplerID();

	void Barrier(GLbitfield b);
};
