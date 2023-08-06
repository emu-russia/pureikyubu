#include "pch.h"

// Now we're putting all the drawing on the shoulders of the graphics API. If we get bored, we can make a software rasterizer.

namespace GX
{
    void GXCore::GL_RenderTriangle(
        const Vertex* v0,
        const Vertex* v1,
        const Vertex* v2)
    {
        const Vertex* vn[3] = { v0, v1, v2 };
        float mv[3][3];    // result vectors

        // position transform
        ApplyModelview(mv[0], v0->pos);
        ApplyModelview(mv[1], v1->pos);
        ApplyModelview(mv[2], v2->pos);

#ifndef WIREFRAME
        if (state.xf.numTex && tID[0])
        {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, tID[0]->bind);
        }
#endif

        // render triangle
        glBegin(GL_TRIANGLES);
        {
            for (int v = 0; v < 3; v++)
            {
#ifndef WIREFRAME
                // color hack
                if (state.xf.numColors)
                {
                    DoLights(vn[v]);
                    glColor4ub(rasca[0].R, rasca[0].G, rasca[0].B, rasca[0].A);
                }

                // texture hack
                if (state.xf.numTex && tID[0])
                {
                    DoTexGen(vn[v]);
                    tgout[0].out[0] *= tID[0]->ds;
                    tgout[0].out[1] *= tID[0]->dt;
                    glTexCoord2fv(tgout[0].out);
                }
#endif

#ifdef WIREFRAME
                glColor3ub(0, 255, 255);
#endif
                glVertex3fv(mv[v]);
            }
        }
        glEnd();

        tris++;
    }

    void GXCore::GL_RenderLine(
        const Vertex* v0,
        const Vertex* v1)
    {
        const Vertex* vn[2] = { v0, v1 };
        float mv[2][3];    // result vectors

        // position transform
        ApplyModelview(&mv[0][0], v0->pos);
        ApplyModelview(&mv[1][0], v1->pos);

        // render line
        //glEnable(GL_LINE_SMOOTH);
        glBegin(GL_LINES);
        {
            DoLights(vn[0]);
            glColor4ub(rasca[0].R, rasca[0].G, rasca[0].B, 0);
            glVertex3f(mv[0][0], mv[0][1], mv[0][2]);
            DoLights(vn[1]);
            glColor4ub(rasca[0].R, rasca[0].G, rasca[0].B, 0);
            glVertex3f(mv[1][0], mv[1][1], mv[1][2]);
        }
        glEnd();
        //glDisable(GL_LINE_SMOOTH);

        lines++;
    }

    void GXCore::GL_RenderPoint(
        const Vertex* v0)
    {
        float mv[3];    // result vectors

        // position transform
        ApplyModelview(&mv[0], v0->pos);

        // render triangle
        glBegin(GL_POINTS);
        {
            DoLights(v0);
            glColor3ub(rasca[0].R, rasca[0].G, rasca[0].B);
            glVertex3f(mv[0], mv[1], mv[2]);
        }
        glEnd();

        pts++;
    }
}
