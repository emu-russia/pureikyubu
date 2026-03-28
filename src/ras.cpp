#include "pch.h"

// Now we're putting all the drawing on the shoulders of the graphics API. If we get bored, we can make a software rasterizer.

namespace GFX
{
	void Rasterizer::RAS_Begin(RAS_Primitive prim, size_t vtx_num)
	{
		switch (prim)
		{
			case RAS_QUAD:
				ras_use_texture = true;
				break;
			case RAS_QUAD_STRIP:
				ras_use_texture = true;
				break;
			case RAS_TRIANGLE:
				ras_use_texture = true;
				break;
			case RAS_TRIANGLE_STRIP:
				ras_use_texture = true;
				break;
			case RAS_TRIANGLE_FAN:
				ras_use_texture = true;
				break;
			case RAS_LINE:
				ras_use_texture = false;
				break;
			case RAS_LINE_STRIP:
				ras_use_texture = false;
				break;
			case RAS_POINT:
				ras_use_texture = false;
				break;
		}

		// texture hack
		if (ras_use_texture && gfx->xf->xf.numTex && gfx->tx->tID[0])
		{
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, gfx->tx->tID[0]->bind);
		}

		switch (prim)
		{
			case RAS_QUAD:
				glBegin(ras_wireframe ? GL_LINE_LOOP : GL_QUADS);
				break;
			case RAS_QUAD_STRIP:
				glBegin(ras_wireframe ? GL_LINE_LOOP : GL_QUAD_STRIP);
				break;
			case RAS_TRIANGLE:
				glBegin(ras_wireframe ? GL_LINE_LOOP : GL_TRIANGLES);
				break;
			case RAS_TRIANGLE_STRIP:
				if (ras_wireframe) {
					glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
				}
				glBegin(GL_TRIANGLE_STRIP);
				break;
			case RAS_TRIANGLE_FAN:
				if (ras_wireframe) {
					glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
				}
				glBegin(GL_TRIANGLE_FAN);
				break;
			case RAS_LINE:
				glBegin(GL_LINES);
				break;
			case RAS_LINE_STRIP:
				glBegin(GL_LINE_STRIP);
				break;
			case RAS_POINT:
				glBegin(GL_POINTS);
				break;
		}
	}

	void Rasterizer::RAS_End()
	{
		glEnd();
	}

	void Rasterizer::RAS_SendVertex(const Vertex* v)
	{
		float mv[3]{};
		
		gfx->xf->XF_ApplyModelview(v, mv, v->Position);

		// The color is transferred via Uniforms

		if (gfx->xf->xf.numColors != 0)
		{
			gfx->xf->XF_DoLights(v);

			if (ras_wireframe) {
				glColor3ub(0, 255, 255);
			}
			else {
				glColor4ub(gfx->xf->colora[0].R, gfx->xf->colora[0].G, gfx->xf->colora[0].B, gfx->xf->colora[0].A);
			}
		}

		// texture hack
		if (ras_use_texture && gfx->xf->xf.numTex && gfx->tx->tID[0])
		{
			gfx->xf->XF_DoTexGen(v);
			gfx->xf->tgout[0].out[0] *= gfx->tx->tID[0]->ds;
			gfx->xf->tgout[0].out[1] *= gfx->tx->tID[0]->dt;
			glTexCoord2fv(gfx->xf->tgout[0].out);
		}

		glVertex3fv(mv);
	}

	Rasterizer::Rasterizer(HWConfig* config, GFXCore* parent_gfx)
	{
		gfx = parent_gfx;
	}

	Rasterizer::~Rasterizer()
	{
	}

	void Rasterizer::loadRASReg(size_t index, uint32_t value)
	{
		gfx->pe->loadPEReg(index, value);
	}
}