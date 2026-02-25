// Transform Unit
#include "pch.h"

using namespace Debug;

#define NO_VIEWPORT

namespace GX
{
	bool GXCore::XF_LightColorEnabled(int chan, int light)
	{
		switch (chan)
		{
			case 0: return xf.colorControl[chan].Light0;
			case 1: return xf.colorControl[chan].Light1;
			case 2: return xf.colorControl[chan].Light2;
			case 3: return xf.colorControl[chan].Light3;
			case 4: return xf.colorControl[chan].Light4;
			case 5: return xf.colorControl[chan].Light5;
			case 6: return xf.colorControl[chan].Light6;
			case 7: return xf.colorControl[chan].Light7;
			default: return false;
		}
	}

	bool GXCore::XF_LightAlphaEnabled(int chan, int light)
	{
		switch (chan)
		{
			case 0: return xf.alphaControl[chan].Light0;
			case 1: return xf.alphaControl[chan].Light1;
			case 2: return xf.alphaControl[chan].Light2;
			case 3: return xf.alphaControl[chan].Light3;
			case 4: return xf.alphaControl[chan].Light4;
			case 5: return xf.alphaControl[chan].Light5;
			case 6: return xf.alphaControl[chan].Light6;
			case 7: return xf.alphaControl[chan].Light7;
			default: return false;
		}
	}

	// normalize (clamp vector to 1.0 length)
	void GXCore::VECNormalize(float vec[3])
	{
		float d = (float)sqrt(vec[0] * vec[0] + vec[1] * vec[1] + vec[2] * vec[2]);

		vec[0] /= d;
		vec[1] /= d;
		vec[2] /= d;
	}

	// perform position transform
	void GXCore::XF_ApplyModelview(const Vertex* v, float* out, const float* in)
	{
		float* mx = &xf.mvTexMtx[v->matIdx0.PosNrmMatIdx * 4];

		out[0] = in[0] * mx[0] + in[1] * mx[1] + in[2] * mx[2] + mx[3];
		out[1] = in[0] * mx[4] + in[1] * mx[5] + in[2] * mx[6] + mx[7];
		out[2] = in[0] * mx[8] + in[1] * mx[9] + in[2] * mx[10] + mx[11];
	}

	// perform normal transform
	// matrix must be the inverse transpose of the modelview matrix
	void GXCore::NormalTransform(const Vertex* v, float* out, const float* in)
	{
		float* mx = &xf.nrmMtx[v->matIdx0.PosNrmMatIdx * 3];

		out[0] = in[0];
		out[1] = in[1];
		out[2] = in[2];

		//out[0] = in[0] * mx[0] + in[1] * mx[1] + in[2] * mx[2];
		//out[1] = in[0] * mx[3] + in[1] * mx[4] + in[2] * mx[5];
		//out[2] = in[0] * mx[6] + in[1] * mx[7] + in[2] * mx[8];

		VECNormalize(out);
	}

	#define CLAMP(n)                \
	{                               \
		if(n <-1.0f) n =-1.0f;      \
		if(n > 1.0f) n = 1.0f;      \
	}

	#define CLAMP0(n)               \
	{                               \
		if(n < 0.0f) n = 0.0f;      \
		if(n > 1.0f) n = 1.0f;      \
	}

	// color0 only calculation
	void GXCore::XF_DoLights(const Vertex* v)
	{
		float vpos[3], vnrm[3];
		float col[3], res[3];
		float mat[3], amb[3];
		float illum[3];

		// TODO: Second time? :/
		XF_ApplyModelview(v, vpos, v->Position);

		for (uint32_t ncol = 0; ncol < xf.numColors; ncol++) {

			// -------------------------------------------------------------------

			//
			// calculate color for channel 0
			//

			// convert vertex color to [0, 1] interval
			col[0] = (float)v->Col[ncol].R / 255.0f;
			col[1] = (float)v->Col[ncol].G / 255.0f;
			col[2] = (float)v->Col[ncol].B / 255.0f;

			// select material color
			if (xf.colorControl[ncol].MatSrc == 0)
			{
				mat[0] = (float)xf.material[ncol].R / 255.0f;
				mat[1] = (float)xf.material[ncol].G / 255.0f;
				mat[2] = (float)xf.material[ncol].B / 255.0f;
			}
			else
			{
				mat[0] = col[0];
				mat[1] = col[1];
				mat[2] = col[2];
			}

			// calculate light function
			if (xf.colorControl[ncol].LightFunc)
			{
				int n;

				// select ambient color
				if (xf.colorControl[ncol].AmbSrc == 0)
				{
					amb[0] = (float)xf.ambient[ncol].R / 255.0f;
					amb[1] = (float)xf.ambient[ncol].G / 255.0f;
					amb[2] = (float)xf.ambient[ncol].B / 255.0f;
				}
				else
				{
					amb[0] = col[0];
					amb[1] = col[1];
					amb[2] = col[2];
				}

				illum[0] = illum[1] = illum[2] = 0.0f;

				// calculate lights
				for (n = 0; n < 8; n++)
				{
					// check light mask
					if (XF_LightColorEnabled(ncol, n))
					{
						// light color
						col[0] = (float)xf.light[n].rgba.R / 255.0f;
						col[1] = (float)xf.light[n].rgba.G / 255.0f;
						col[2] = (float)xf.light[n].rgba.B / 255.0f;

						// calculate diffuse lighting
						switch (xf.colorControl[ncol].DiffuseAtten)
						{
							case 0:         // identity
								illum[0] += col[0];
								illum[1] += col[1];
								illum[2] += col[2];
								break;

							case 1:         // signed
							case 2:         // clamped
							{
								float dp, dir[3];

								// light direction vector
								dir[0] = xf.light[n].lpx[0] - vpos[0];
								dir[1] = xf.light[n].lpx[1] - vpos[1];
								dir[2] = xf.light[n].lpx[2] - vpos[2];

								// normalize light direction vector
								VECNormalize(dir);

								// normal transformation
								NormalTransform(v, vnrm, v->Normal);

								// dot product of normal and light
								dp = vnrm[0] * dir[0] +
									vnrm[1] * dir[1] +
									vnrm[2] * dir[2];

								// clamp dot product
								if (xf.colorControl[ncol].DiffuseAtten == 2)
								{
									CLAMP0(dp);
								}

								// multiply by light color
								illum[0] += dp * col[0];
								illum[1] += dp * col[1];
								illum[2] += dp * col[2];
								break;
							}
						}

						// diffuse angle and distance attenuation
						// NOT Implemented !!

						// specular
						// NOT Implemented !!
					}
				}

				// clamp to [-1, 1] interval
				CLAMP(illum[0]);
				CLAMP(illum[1]);
				CLAMP(illum[2]);

				// add ambient color
				illum[0] += amb[0];
				illum[1] += amb[1];
				illum[2] += amb[2];

				// clamp total illum to [0, 1]
				CLAMP0(illum[0]);
				CLAMP0(illum[1]);
				CLAMP0(illum[2]);
			}
			else
			{
				// no light function, use material color
				illum[0] = illum[1] = illum[2] = 1.0f;
			}

			// finalize
			res[0] = mat[0] * illum[0];
			res[1] = mat[1] * illum[1];
			res[2] = mat[2] * illum[2];

			// clamp result to [0, 1]
			CLAMP0(res[0]);
			CLAMP0(res[1]);
			CLAMP0(res[2]);

			// write back result
			colora[ncol].R = (uint8_t)(res[0] * 255.0f);
			colora[ncol].G = (uint8_t)(res[1] * 255.0f);
			colora[ncol].B = (uint8_t)(res[2] * 255.0f);

			// -------------------------------------------------------------------

			//
			// calculate alpha for channel 0
			//

			// convert vertex color to [0, 1] interval
			col[0] = (float)v->Col[ncol].A / 255.0f;

			// select material color
			if (xf.alphaControl[ncol].MatSrc == 0)
			{
				mat[0] = (float)xf.material[ncol].A / 255.0f;
			}
			else
			{
				mat[0] = col[0];
			}

			// calculate light function
			if (xf.alphaControl[ncol].LightFunc)
			{
				int n;

				// select ambient color
				if (xf.alphaControl[ncol].AmbSrc == 0)
				{
					amb[0] = (float)xf.ambient[ncol].A / 255.0f;
				}
				else
				{
					amb[0] = col[0];
				}

				illum[0] = 0.0f;

				// calculate lights
				for (n = 0; n < 8; n++)
				{
					// check light mask
					if (XF_LightAlphaEnabled(ncol, n))
					{
						// light color
						col[0] = (float)xf.light[n].rgba.A / 255.0f;

						// calculate diffuse lighting
						switch (xf.alphaControl[ncol].DiffuseAtten)
						{
							case 0:         // identity
								illum[0] += col[0];
								break;

							case 1:         // signed
							case 2:         // clamped
							{
								float dp, dir[3];

								// light direction vector
								dir[0] = xf.light[n].lpx[0] - vpos[0];
								dir[1] = xf.light[n].lpx[1] - vpos[1];
								dir[2] = xf.light[n].lpx[2] - vpos[2];

								// normalize light direction vector
								VECNormalize(dir);

								// normal transformation
								NormalTransform(v, vnrm, v->Normal);

								// dot product of normal and light
								dp = vnrm[0] * dir[0] +
									vnrm[1] * dir[1] +
									vnrm[2] * dir[2];

								// clamp dot product
								if (xf.alphaControl[ncol].DiffuseAtten == 2)
								{
									CLAMP0(dp);
								}

								// multiply by light color
								illum[0] += dp * col[0];
								break;
							}
						}

						// diffuse angle and distance attenuation
						// NOT Implemented !!

						// specular
						// NOT Implemented !!
					}
				}

				// clamp to [-1, 1] interval
				CLAMP(illum[0]);

				// add ambient color
				illum[0] += amb[0];

				// clamp total illum to [0, 1]
				CLAMP0(illum[0]);
			}
			else
			{
				// no light function, use material color
				illum[0] = 1.0f;
			}

			// finalize
			res[0] = mat[0] * illum[0];

			// clamp result to [0, 1]
			CLAMP0(res[0]);

			// write back result
			colora[ncol].A = (uint8_t)(res[0] * 255.0f);
		}
	}

	// generate NUMTEX coordinates
	void GXCore::XF_DoTexGen(const Vertex* v)
	{
		float   in[4], q;
		float* mx = nullptr;

		if (xf.numTex == 0)
		{
			mx = &xf.mvTexMtx[v->matIdx0.Tex0MatIdx * 4];
			in[0] = v->TexCoord[0][0];
			in[1] = v->TexCoord[0][1];
			in[2] = 1.0f;
			in[3] = 1.0f;
		}

		for (unsigned n = 0; n < xf.numTex; n++)
		{
			if (xf.tex[n].type == 0)
			{
				// select inrow
				switch (xf.tex[n].src_row)
				{
					case XF_TEXGEN_INROW_POSMTX:
					{
						in[0] = v->Position[0];
						in[1] = v->Position[1];
						in[2] = v->Position[2];
						in[3] = 1.0f;
					}
					break;

					case XF_TEXGEN_INROW_NORMAL:
					{
						in[0] = v->Normal[0];
						in[1] = v->Normal[1];
						in[2] = v->Normal[2];
						in[3] = 1.0f;
					}
					break;

					case XF_TEXGEN_INROW_TEX0:
					{
						mx = &xf.mvTexMtx[v->matIdx0.Tex0MatIdx * 4];
						in[0] = v->TexCoord[0][0];
						in[1] = v->TexCoord[0][1];
						in[2] = 1.0f;
						in[3] = 1.0f;
					}
					break;

					case XF_TEXGEN_INROW_TEX1:
					{
						mx = &xf.mvTexMtx[v->matIdx0.Tex1MatIdx * 4];
						in[0] = v->TexCoord[1][0];
						in[1] = v->TexCoord[1][1];
						in[2] = 1.0f;
						in[3] = 1.0f;
					}
					break;

					case XF_TEXGEN_INROW_TEX2:
					{
						mx = &xf.mvTexMtx[v->matIdx0.Tex2MatIdx * 4];
						in[0] = v->TexCoord[2][0];
						in[1] = v->TexCoord[2][1];
						in[2] = 1.0f;
						in[3] = 1.0f;
					}
					break;

					case XF_TEXGEN_INROW_TEX3:
					{
						mx = &xf.mvTexMtx[v->matIdx0.Tex3MatIdx * 4];
						in[0] = v->TexCoord[3][0];
						in[1] = v->TexCoord[3][1];
						in[2] = 1.0f;
						in[3] = 1.0f;
					}
					break;

					case XF_TEXGEN_INROW_TEX4:
					{
						mx = &xf.mvTexMtx[v->matIdx1.Tex4MatIdx * 4];
						in[0] = v->TexCoord[4][0];
						in[1] = v->TexCoord[4][1];
						in[2] = 1.0f;
						in[3] = 1.0f;
					}
					break;

					case XF_TEXGEN_INROW_TEX5:
					{
						mx = &xf.mvTexMtx[v->matIdx1.Tex5MatIdx * 4];
						in[0] = v->TexCoord[5][0];
						in[1] = v->TexCoord[5][1];
						in[2] = 1.0f;
						in[3] = 1.0f;
					}
					break;

					case XF_TEXGEN_INROW_TEX6:
					{
						mx = &xf.mvTexMtx[v->matIdx1.Tex6MatIdx * 4];
						in[0] = v->TexCoord[6][0];
						in[1] = v->TexCoord[6][1];
						in[2] = 1.0f;
						in[3] = 1.0f;
					}
					break;

					case XF_TEXGEN_INROW_TEX7:
					{
						mx = &xf.mvTexMtx[v->matIdx1.Tex7MatIdx * 4];
						in[0] = v->TexCoord[7][0];
						in[1] = v->TexCoord[7][1];
						in[2] = 1.0f;
						in[3] = 1.0f;
					}
					break;
				}

				// Hmmm :/
				if (mx == nullptr) {
					mx = &xf.mvTexMtx[v->matIdx0.Tex0MatIdx * 4];
				}

				// st or stq ?
				if (xf.tex[n].projection)
				{
					tgout[n].out[0] = in[0] * mx[0] + in[1] * mx[1] + in[2] * mx[2] + mx[3];
					tgout[n].out[1] = in[0] * mx[4] + in[1] * mx[5] + in[2] * mx[6] + mx[7];
					q = in[0] * mx[8] + in[1] * mx[9] + in[2] * mx[10] + mx[11];
					tgout[n].out[0] /= q;
					tgout[n].out[1] /= q;
				}
				else
				{
					tgout[n].out[0] = in[0] * mx[0] + in[1] * mx[1] + mx[2] + mx[3];
					tgout[n].out[1] = in[0] * mx[4] + in[1] * mx[5] + mx[6] + mx[7];
				}

				// dual-transform
			}
		}

		//tgout[0].out[1] /= 1.33333;
	}



	// load projection matrix
	void GXCore::GL_SetProjection(float* mtx)
	{
		glMatrixMode(GL_PROJECTION);
		glLoadMatrixf((GLfloat*)mtx);
		glMatrixMode(GL_MODELVIEW);
	}

	void GXCore::GL_SetViewport(int x, int y, int w, int h, float znear, float zfar)
	{
		//h += 32;
//#ifndef NO_VIEWPORT
		glViewport(x, scr_h - (h + y), w, h);
		glDepthRange(znear, zfar);
//#endif
	}

	// index range = 0000..FFFF
	// reg size = 32 bit
	void GXCore::loadXFRegs(size_t startIdx, size_t amount, FifoProcessor* gxfifo)
	{
		xfLoads += (uint32_t)amount;

		if (GpRegsLog)
		{
			Report(Channel::GP, "XF load, start index: %04X, n : %i\n", startIdx, amount);
		}

		// load geometry matrix
		if ((startIdx >= XF_MATRIX_MEMORY_ID) && (startIdx < (XF_MATRIX_MEMORY_ID + XF_MATRIX_MEMORY_SIZE)))
		{
			for (size_t i = 0; i < amount; i++)
			{
				xf.mvTexMtx[(startIdx - XF_MATRIX_MEMORY_ID) + i] = gxfifo->ReadFloat();
			}
		}
		// load normal matrix
		else if ((startIdx >= XF_NORMAL_MATRIX_MEMORY_ID) && (startIdx < (XF_NORMAL_MATRIX_MEMORY_ID + XF_NORMAL_MATRIX_MEMORY_SIZE)))
		{
			for (size_t i = 0; i < amount; i++)
			{
				xf.nrmMtx[(startIdx - XF_NORMAL_MATRIX_MEMORY_ID) + i] = gxfifo->ReadFloat();
			}
		}
		// load post-trans matrix
		else if ((startIdx >= XF_DUALTEX_MATRIX_MEMORY_ID) && (startIdx < (XF_DUALTEX_MATRIX_MEMORY_ID + XF_DUALTEX_MATRIX_MEMORY_SIZE)))
		{
			for (size_t i = 0; i < amount; i++)
			{
				xf.dualTexMtx[(startIdx - XF_DUALTEX_MATRIX_MEMORY_ID) + i] = gxfifo->ReadFloat();
			}
		}
		else switch (startIdx)
		{
			case XF_ERROR_ID:
				xf.error = gxfifo->Read32();
				break;
			case XF_DIAGNOSTICS_ID:
				xf.diagnostics = gxfifo->Read32();
				break;
			case XF_STATE0_ID:
				xf.state[0] = gxfifo->Read32();
				break;
			case XF_STATE1_ID:
				xf.state[1] = gxfifo->Read32();
				break;
			case XF_CLOCK_ID:
				xf.clock = gxfifo->Read32();
				break;
			case XF_CLIP_DISABLE_ID:
				// TODO: How does this affect Culling in the Setup Unit?
				xf.clipDisable.bits = gxfifo->Read32();
				break;
			case XF_PERF0_ID:
				xf.perf[0] = gxfifo->Read32();
				break;
			case XF_PERF1_ID:
				xf.perf[1] = gxfifo->Read32();
				break;

			//
			// set matrix index
			//

			case XF_MATINDEX_A_ID:
				xf.matIdxA.bits = gxfifo->Read32();
				break;

			case XF_MATINDEX_B_ID:
				xf.matIdxB.bits = gxfifo->Read32();
				break;

			//
			// load projection matrix (TODO: unaligned writes)
			//

			case XF_PROJECTION_A_ID:
			{
				float pMatrix[7];

				if (amount != 7) {
					Halt("Partial loading of the projection matrix is not implemented\n");
				}

				pMatrix[0] = gxfifo->ReadFloat();
				pMatrix[1] = gxfifo->ReadFloat();
				pMatrix[2] = gxfifo->ReadFloat();
				pMatrix[3] = gxfifo->ReadFloat();
				pMatrix[4] = gxfifo->ReadFloat();
				pMatrix[5] = gxfifo->ReadFloat();
				pMatrix[6] = gxfifo->ReadFloat();

				float Matrix[4][4];
				if (pMatrix[6] == 0)
				{
					Matrix[0][0] = pMatrix[0];
					Matrix[1][0] = 0.0f;
					Matrix[2][0] = pMatrix[1];
					Matrix[3][0] = 0.0f;
					Matrix[0][1] = 0.0f;
					Matrix[1][1] = pMatrix[2];
					Matrix[2][1] = pMatrix[3];
					Matrix[3][1] = 0.0f;
					Matrix[0][2] = 0.0f;
					Matrix[1][2] = 0.0f;
					Matrix[2][2] = pMatrix[4];
					Matrix[3][2] = pMatrix[5];
					Matrix[0][3] = 0.0f;
					Matrix[1][3] = 0.0f;
					Matrix[2][3] = -1.0f;
					Matrix[3][3] = 0.0f;
				}
				else
				{
					Matrix[0][0] = pMatrix[0];
					Matrix[1][0] = 0.0f;
					Matrix[2][0] = 0.0f;
					Matrix[3][0] = pMatrix[1];
					Matrix[0][1] = 0.0f;
					Matrix[1][1] = pMatrix[2];
					Matrix[2][1] = 0.0f;
					Matrix[3][1] = pMatrix[3];
					Matrix[0][2] = 0.0f;
					Matrix[1][2] = 0.0f;
					Matrix[2][2] = pMatrix[4];
					Matrix[3][2] = pMatrix[5];
					Matrix[0][3] = 0.0f;
					Matrix[1][3] = 0.0f;
					Matrix[2][3] = 0.0f;
					Matrix[3][3] = 1.0f;
				}

				GL_SetProjection((float*)Matrix);
			}
			return;

			//
			// load viewport configuration (TODO: unaligned writes)
			// 

			case XF_VIEWPORT_SCALE_X_ID:
			{
				float w, h, x, y, zf, zn;

				if (amount != 6) {
					Halt("Partial loading of the viewport settings is not implemented\n");
				}

				//
				// read coefficients
				//

				xf.viewportScale[0] = gxfifo->ReadFloat();   // w / 2
				xf.viewportScale[1] = gxfifo->ReadFloat();   // -h / 2
				xf.viewportScale[2] = gxfifo->ReadFloat();   // ZMAX * (zfar - znear)

				xf.viewportOffset[0] = gxfifo->ReadFloat();    // x + w/2 + 342
				xf.viewportOffset[1] = gxfifo->ReadFloat();    // y + h/2 + 342
				xf.viewportOffset[2] = gxfifo->ReadFloat();    // ZMAX * zfar

				//
				// convert them to human usable form
				//

				w = xf.viewportScale[0] * 2;
				h = -xf.viewportScale[1] * 2;
				x = xf.viewportOffset[0] - xf.viewportScale[0] - 342;
				y = xf.viewportOffset[1] + xf.viewportScale[1] - 342;
				zf = xf.viewportOffset[2] / 16777215.0f;
				zn = -((xf.viewportScale[2] / 16777215.0f) - zf);

				//GFXError("viewport (%.2f, %.2f)-(%.2f, %.2f), %f, %f", x, y, w, h, zn, zf);
				GL_SetViewport((int)x, (int)y, (int)w, (int)h, zn, zf);
			}
			return;

			//
			// load light object (TODO: unaligned writes not supported)
			//

			case XF_LIGHT0_ID:
			case XF_LIGHT1_ID:
			case XF_LIGHT2_ID:
			case XF_LIGHT3_ID:
			case XF_LIGHT4_ID:
			case XF_LIGHT5_ID:
			case XF_LIGHT6_ID:
			case XF_LIGHT7_ID:
			{
				unsigned lnum = (startIdx >> 4) & 7;

				if (amount != 16) {
					Halt("Partial loading of the light object memory is not implemented\n");
				}

				xf.light[lnum].Reserved[0] = gxfifo->Read32();
				xf.light[lnum].Reserved[1] = gxfifo->Read32();
				xf.light[lnum].Reserved[2] = gxfifo->Read32();
				xf.light[lnum].rgba.RGBA = gxfifo->Read32();

				xf.light[lnum].a[0] = gxfifo->ReadFloat();
				xf.light[lnum].a[1] = gxfifo->ReadFloat();
				xf.light[lnum].a[2] = gxfifo->ReadFloat();

				xf.light[lnum].k[0] = gxfifo->ReadFloat();
				xf.light[lnum].k[1] = gxfifo->ReadFloat();
				xf.light[lnum].k[2] = gxfifo->ReadFloat();

				xf.light[lnum].lpx[0] = gxfifo->ReadFloat();
				xf.light[lnum].lpx[1] = gxfifo->ReadFloat();
				xf.light[lnum].lpx[2] = gxfifo->ReadFloat();

				xf.light[lnum].dhx[0] = gxfifo->ReadFloat();
				xf.light[lnum].dhx[1] = gxfifo->ReadFloat();
				xf.light[lnum].dhx[2] = gxfifo->ReadFloat();
			}
			return;

			case XF_INVTXSPEC_ID:
			{
				xf.vtxSpec.bits = gxfifo->Read32();
			}
			return;

			//
			// channel constant color registers
			//

			case XF_AMBIENT0_ID:
				xf.ambient[0].RGBA = gxfifo->Read32();
				break;

			case XF_AMBIENT1_ID:
				xf.ambient[1].RGBA = gxfifo->Read32();
				break;

			case XF_MATERIAL0_ID:
				xf.material[0].RGBA = gxfifo->Read32();
				break;

			case XF_MATERIAL1_ID:
				xf.material[1].RGBA = gxfifo->Read32();
				break;

			//
			// channel control registers
			//

			case XF_COLOR0CNTL_ID:
				xf.colorControl[0].bits = gxfifo->Read32();
				break;

			case XF_COLOR1CNTL_ID:
				xf.colorControl[1].bits = gxfifo->Read32();
				break;

			case XF_ALPHA0CNTL_ID:
				xf.alphaControl[0].bits = gxfifo->Read32();
				break;

			case XF_ALPHA1CNTL_ID:
				xf.alphaControl[1].bits = gxfifo->Read32();
				break;

			//
			// set dualtex enable / disable
			//

			case XF_DUALTEX_ID:
			{
				xf.dualTexTran = gxfifo->Read32();
				//GFXError("dual texgen : %s", (regData[0]) ? ("on") : ("off"));
			}
			return;

			case XF_DUALGEN0_ID:
			case XF_DUALGEN1_ID:
			case XF_DUALGEN2_ID:
			case XF_DUALGEN3_ID:
			case XF_DUALGEN4_ID:
			case XF_DUALGEN5_ID:
			case XF_DUALGEN6_ID:
			case XF_DUALGEN7_ID:
			{
				size_t n = startIdx - XF_DUALGEN0_ID;

				gxfifo->Read32();

				//ASSERT(amount != 1);

				//xfRegs.dual[n].hex = regData[0];

	/*/
				GFXError(
					"set dual for %i:\n"
					"raw: %08X\n"
					"index: %i\n"
					"normalize: %s",
					n,
					regData[0],
					xfRegs.dual[n].dualidx,
					(xfRegs.dual[n].norm) ? ("yes") : ("no")
				);
	/*/
			}
			return;

			//
			// number of output colors
			//

			case XF_NUMCOLS_ID:
				xf.numColors = gxfifo->Read32();
				break;

			//
			// set number of texgens
			//

			case XF_NUMTEX_ID:
				xf.numTex = gxfifo->Read32();
				break;

			// 
			// set texgen configuration
			//

			case XF_TEXGEN0_ID:
			case XF_TEXGEN1_ID:
			case XF_TEXGEN2_ID:
			case XF_TEXGEN3_ID:
			case XF_TEXGEN4_ID:
			case XF_TEXGEN5_ID:
			case XF_TEXGEN6_ID:
			case XF_TEXGEN7_ID:
			{
				unsigned num = startIdx & 7;

				static  const char* prj[] = { "2x4", "3x4" };
				static  const char* inf[] = { "ab11", "abc1" };
				static  const char* type[] = { "regular", "bump", "toon0", "toon1" };
				static  const char* srcrow[] = {
					"xyz",
					"normal",
					"colors",
					"binormal t",
					"binormal b",
					"tex0",
					"tex1",
					"tex2",
					"tex3",
					"tex4",
					"tex5",
					"tex6",
					"tex7",
					"", "", ""
				};

				xf.tex[num].bits = gxfifo->Read32();
			}
			return;

			//
			// not implemented
			//

			default:
			{
				Report(Channel::GP, "Unknown XF load, start index: 0x%04X, count: %i\n", startIdx, amount);

				while (amount--)
				{
					gxfifo->Read32();
				}
			}
		}
	}

}
