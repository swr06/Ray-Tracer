#pragma once

#include <iostream>
#include <string>
#include <glad/glad.h>

namespace GLClasses
{
	class Framebuffer
	{
	public :
		Framebuffer(unsigned int w, unsigned int h, bool has_depth_attachment = true);
		~Framebuffer();

		Framebuffer(const Framebuffer&) = delete;
		Framebuffer operator=(Framebuffer const&) = delete;

		Framebuffer& operator=(Framebuffer&& other)
		{
			std::swap(*this, other);
			return *this;
		}

		Framebuffer(Framebuffer&& v) : m_HasDepthMap(v.m_HasDepthMap)
		{
			m_FBO = v.m_FBO;
			m_TextureAttachment = v.m_TextureAttachment;
			m_FBWidth = v.m_FBWidth;
			m_FBHeight = v.m_FBHeight;

			v.m_FBO = 0;
			v.m_TextureAttachment = 0;
		}

		void Bind() const
		{
			glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
			glViewport(0, 0, m_FBWidth, m_FBHeight);
		}

		void Unbind() const
		{
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		inline GLuint GetTexture() const 
		{
			return m_TextureAttachment;
		}

		inline GLuint GetFramebuffer() const noexcept { return m_FBO; }
		inline unsigned int GetWidth() const noexcept { return m_FBWidth; }
		inline unsigned int GetHeight() const noexcept { return m_FBHeight; }

	private :
		void CreateFramebuffer(int w, int h);

		GLuint m_FBO; // The Framebuffer object
		GLuint m_TextureAttachment; // The actual texture attachment
		int m_FBWidth;
		int m_FBHeight;
		const bool m_HasDepthMap;
		float m_Exposure = 0.0f;
	};
}