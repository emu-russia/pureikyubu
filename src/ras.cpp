#include "pch.h"

// Now we're putting all the drawing on the shoulders of the graphics API. If we get bored, we can make a software rasterizer.

namespace GX
{

	void GXCore::RAS_Begin(RAS_Primitive prim, size_t vtx_num)
	{
		switch (prim)
		{
			case RAS_QUAD:
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
		if (ras_use_texture && state.xf.numTex && tID[0])
		{
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, tID[0]->bind);
		}

		switch (prim)
		{
			case RAS_QUAD:
				glBegin(GL_QUADS);
				break;
			case RAS_TRIANGLE:
				glBegin(GL_TRIANGLES);
				break;
			case RAS_TRIANGLE_STRIP:
				glBegin(GL_TRIANGLE_STRIP);
				break;
			case RAS_TRIANGLE_FAN:
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

		last_prim = prim;
		ras_vtx_num = vtx_num;
	}

	void GXCore::RAS_End()
	{
		switch (last_prim)
		{
			case RAS_QUAD:
				tris += (ras_vtx_num / 4) / 2;
				break;
			case RAS_TRIANGLE:
				tris += ras_vtx_num / 3;
				break;
			case RAS_TRIANGLE_STRIP:
				tris += ras_vtx_num - 2;
				break;
			case RAS_TRIANGLE_FAN:
				tris += ras_vtx_num - 2;
				break;
			case RAS_LINE:
				lines += ras_vtx_num / 2;
				break;
			case RAS_LINE_STRIP:
				lines += ras_vtx_num - 1;
				break;
			case RAS_POINT:
				pts += ras_vtx_num;
				break;
		}
		glEnd();
	}

	void GXCore::RAS_SendVertex(const Vertex* v)
	{
		float mv[3];
		
		XF_ApplyModelview(mv, v->pos);

		// color hack
		if (state.xf.numColors)
		{
			XF_DoLights(v);
			glColor4ub(rasca[0].R, rasca[0].G, rasca[0].B, rasca[0].A);
		}

		// texture hack
		if (ras_use_texture && state.xf.numTex && tID[0])
		{
			XF_DoTexGen(v);
			tgout[0].out[0] *= tID[0]->ds;
			tgout[0].out[1] *= tID[0]->dt;
			glTexCoord2fv(tgout[0].out);
		}

		glVertex3fv(mv);
	}
}
