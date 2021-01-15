#include "Framebuffer.h"

#ifndef NULL
	#define NULL 0
#endif

namespace GLClasses
{
    Framebuffer::Framebuffer(unsigned int w, unsigned int h, bool has_depth_attachment) :
        m_FBO(0), m_FBWidth(w), m_FBHeight(h), m_HasDepthMap(has_depth_attachment)
    {
        CreateFramebuffer(w, h);
    }

	Framebuffer::~Framebuffer()
	{
		glDeleteFramebuffers(1, &m_FBO);
	}

	void Framebuffer::CreateFramebuffer(int w, int h)
	{
        glGenFramebuffers(1, &m_FBO);
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);

        glGenTextures(1, &m_TextureAttachment);
        glBindTexture(GL_TEXTURE_2D, m_TextureAttachment);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_TextureAttachment, 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            std::cout << "Fatal error! Framebuffer creation failed!";
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
}