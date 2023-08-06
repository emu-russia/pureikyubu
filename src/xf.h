// Transform Unit

// This module deals with geometric transformation and lighting of vertices that come from CP.
// All parameters (matrices) are stored in a special memory (XF).
// In the real Flipper XF is made from microcode ROM, but is still part of a fixed pipeline.
// In an emulator, XF can be done entirely programmatically (as in the current old and crooked implementation), or using vertex shaders.

#pragma once

namespace GX
{

	// XF Registers

	enum class XFRegister
	{
		// 0x0000...0x03FF - ModelView/Texture Matrix memory. This block is formed by the matrix memory. Its address range is 0 to 1k, but only 256 entries are used. This memory is organized in a 64 entry by four 32b words.

		XF_MATRIX_MEMORY_ID = 0x0000,
		XF_MATRIX_MEMORY_SIZE = (64 * 4),

		// 0x0400...0x04FF - Normal matrix memory. This block of memory is the normal matrix memory. It is organized as 32 rows of 3 words.

		XF_NORMAL_MATRIX_MEMORY_ID = 0x0400,
		XF_NORMAL_MATRIX_MEMORY_SIZE = (32 * 3),

		// 0x0500...0x05FF - Texture post-transform matrix memory. This block of memory holds the dual texture transform matrices. The format is identical to the first block of matrix memory. 
		// There are also 64 rows of 4 words for these matrices

		XF_DUALTEX_MATRIX_MEMORY_ID = 0x0500,
		XF_DUALTEX_MATRIX_MEMORY_SIZE = (64 * 4),

		// Lighting parameters memory

		XF_LIGHT_MEMORY_ID = 0x0600,
		XF_LIGHT_DATA_SIZE = 0x10,
		XF_LIGHT_MEMORY_SIZE = (XF_LIGHT_DATA_SIZE * 8),

		// Light0 parameters

		XF_LIGHT0_ID = 0x0600,			// reserved
		XF_LIGHT0_Reserved1 = 0x0601,	// reserved
		XF_LIGHT0_Reserved2 = 0x0602,	// reserved
		XF_LIGHT0_RGBA_ID = 0x0603,  // RGBA (8b/comp)
		XF_LIGHT0_A0_ID = 0x0604,  // cos atten a0
		XF_LIGHT0_A1_ID = 0x0605,  // cos atten a1
		XF_LIGHT0_A2_ID = 0x0606,  // cos atten a2
		XF_LIGHT0_K0_ID = 0x0607,  // dist atten k0
		XF_LIGHT0_K1_ID = 0x0608,  // dist atten k1
		XF_LIGHT0_K2_ID = 0x0609,  // dist atten k2
		XF_LIGHT0_LPX_ID = 0x060A,  // x light pos, or inf ldir x
		XF_LIGHT0_LPY_ID = 0x060B,  // y light pos, or inf ldir y
		XF_LIGHT0_LPZ_ID = 0x060C,  // z light pos, or inf ldir z
		XF_LIGHT0_DHX_ID = 0x060D,  // light dir x, or 1/2 angle x
		XF_LIGHT0_DHY_ID = 0x060E,  // light dir y, or 1/2 angle y
		XF_LIGHT0_DHZ_ID = 0x060F,  // light dir z, or 1/2 angle z

		// 0x0610-0x067f: Parameters for Light1-Light7. See Light0 data.

		XF_LIGHT1_ID = 0x0610,
		XF_LIGHT2_ID = 0x0620,
		XF_LIGHT3_ID = 0x0630,
		XF_LIGHT4_ID = 0x0640,
		XF_LIGHT5_ID = 0x0650,
		XF_LIGHT6_ID = 0x0660,
		XF_LIGHT7_ID = 0x0670,

		// XF 0x0680...0x07FF are reserved

		// Other general XF parameters (>= 0x1000)

		XF_ERROR_ID = 0x1000,
		XF_DIAGNOSTICS_ID = 0x1001,
		XF_STATE0_ID = 0x1002,
		XF_STATE1_ID = 0x1003,
		XF_CLOCK_ID = 0x1004,
		XF_CLIP_DISABLE_ID = 0x1005,
		XF_PERF0_ID = 0x1006,
		XF_PERF1_ID = 0x1007,

		XF_INVTXSPEC_ID = 0x1008,
		XF_NUMCOLS_ID = 0x1009,  // Selects the number of output colors
		XF_AMBIENT0_ID = 0x100A,  // RGBA (8b/comp) ambient color 0
		XF_AMBIENT1_ID = 0x100B,  // RGBA (8b/comp) ambient color 1
		XF_MATERIAL0_ID = 0x100C,  // RGBA (8b/comp) material color 0
		XF_MATERIAL1_ID = 0x100D,  // RGBA (8b/comp) material color 1
		XF_COLOR0CNTL_ID = 0x100E,  // COLOR0 channel control
		XF_COLOR1CNTL_ID = 0x100F,  // COLOR1 channel control
		XF_ALPHA0CNTL_ID = 0x1010,  // ALPHA0 channel control
		XF_ALPHA1CNTL_ID = 0x1011,  // ALPHA1 channel control

		XF_DUALTEX_ID = 0x1012,  // enable tex post-transform
		XF_MATINDEX_A_ID = 0x1018,  // Position / Tex coord 0-3 mat index
		XF_MATINDEX_B_ID = 0x1019,  // Tex coord 4-7 mat index
		XF_VIEWPORT_SCALE_X_ID = 0x101A,
		XF_VIEWPORT_SCALE_Y_ID = 0x101B,
		XF_VIEWPORT_SCALE_Z_ID = 0x101C,
		XF_VIEWPORT_OFFSET_X_ID = 0x101D,
		XF_VIEWPORT_OFFSET_Y_ID = 0x101E,
		XF_VIEWPORT_OFFSET_Z_ID = 0x101F,

		// Projection matrix parameters

		XF_PROJECTION_A_ID = 0x1020,
		XF_PROJECTION_B_ID = 0x1021,
		XF_PROJECTION_C_ID = 0x1022,
		XF_PROJECTION_D_ID = 0x1023,
		XF_PROJECTION_E_ID = 0x1024,
		XF_PROJECTION_F_ID = 0x1025,
		XF_PROJECT_ORTHO_ID = 0x1026,

		XF_NUMTEX_ID = 0x103F,  // active texgens
		XF_TEXGEN0_ID = 0x1040,
		XF_TEXGEN1_ID = 0x1041,
		XF_TEXGEN2_ID = 0x1042,
		XF_TEXGEN3_ID = 0x1043,
		XF_TEXGEN4_ID = 0x1044,
		XF_TEXGEN5_ID = 0x1045,
		XF_TEXGEN6_ID = 0x1046,
		XF_TEXGEN7_ID = 0x1047,

		// Dual texgen setup

		XF_DUALGEN0_ID = 0x1050,
		XF_DUALGEN1_ID = 0x1051,
		XF_DUALGEN2_ID = 0x1052,
		XF_DUALGEN3_ID = 0x1053,
		XF_DUALGEN4_ID = 0x1054,
		XF_DUALGEN5_ID = 0x1055,
		XF_DUALGEN6_ID = 0x1056,
		XF_DUALGEN7_ID = 0x1057,
	};

#pragma pack(push, 1)

	union ClipDisable
	{
		struct
		{
			unsigned disableDetection : 1;			// When set, disables clipping detection
			unsigned disableTrivialReject : 1;		// When set, disables trivial rejection
			unsigned disablePolyAccel : 1;			// When set, disables cpoly clipping acceleration
		};
		uint32_t bits;
	};

	static_assert (sizeof(ClipDisable) == sizeof(uint32_t), "ClipDisable invalid definition!");

	union InVertexSpec
	{
		struct
		{
			unsigned color0Usage : 2;		// 0: No host supplied color information 1: Host supplied color0 2: Host supplied color0 and color1
			unsigned normalUsage : 2;		// 0: No host supplied normal 1: Host supplied normal 2: Host supplied normal and binormals
			unsigned texCoords : 4;		// 0: No host supplied textures 1: 1 host supplied texture pair (S0, T0) 2-8: 2-8 host supplied texturepairs; 9-15: Reserved
		};
		uint32_t bits;
	};

	static_assert (sizeof(InVertexSpec) == sizeof(uint32_t), "InVertexSpec invalid definition!");

	// Light parameters

	struct Light
	{
		uint32_t Reserved[3];
		Color rgba;		// RGBA (8b/comp)
		float a[3];		// Post-processed cos atten
		float k[3];		// Post-processed dist atten
		float lpx[3];	// Post-processed x,y,z light pos, or inf ldir x,y,z
		float dhx[3];	// Post-processed x,y,z light dir, or 1/2 angle x,y,z
	};

	static_assert (sizeof(Light) == 0x10 * sizeof(uint32_t), "Light invalid definition!");

	union ColorAlphaControl
	{
		struct
		{
			unsigned	MatSrc : 1;         // Material source. 0: use register, 1: Use CP supplied Vertex color/alpha
			unsigned    LightFunc : 1;      // LightFunc. 0: Use 1.0, 1: Use Illum0
			unsigned 	Light0 : 1;         // 1: use light
			unsigned 	Light1 : 1;         // 1: use light
			unsigned 	Light2 : 1;         // 1: use light
			unsigned 	Light3 : 1;         // 1: use light
			unsigned    AmbSrc : 1;         // Ambient source. 0: use register, 1: Use CP supplied Vertex color/alpha
			unsigned    DiffuseAtten : 2;   // DiffuseAtten function. 0: Use 1.0, 1: N.L signed, 2: N.L clamped to [0,1.0]
			unsigned    Atten : 1;          // AttenEnable function. 0: Select 1.0, 1: Select Attenuation fraction
			unsigned    AttenSelect : 1;    // AttenSelect function. 0: Select specular (N.H) attenuation, 1: Select diffuse spotlight (L.Ldir) attenuation
			unsigned 	Light4 : 1;         // 1: use light
			unsigned 	Light5 : 1;         // 1: use light
			unsigned 	Light6 : 1;         // 1: use light
			unsigned 	Light7 : 1;         // 1: use light
		};
		uint32_t bits;
	};

	static_assert (sizeof(ColorAlphaControl) == sizeof(uint32_t), "ColorAlphaControl invalid definition!");

	union MatrixIndex0
	{
		struct
		{
			unsigned 		PosNrmMatIdx : 6;		// Geometry matrix index
			unsigned 		Tex0MatIdx : 6;			// Tex0 matrix index
			unsigned 		Tex1MatIdx : 6;			// Tex1 matrix index
			unsigned 		Tex2MatIdx : 6;			// Tex2 matrix index
			unsigned 		Tex3MatIdx : 6;			// Tex3 matrix index
		};
		uint32_t bits;
	};

	static_assert (sizeof(MatrixIndex0) == sizeof(uint32_t), "MatrixIndex0 invalid definition!");

	union MatrixIndex1
	{
		struct
		{
			unsigned 		Tex4MatIdx : 6;			// Tex4 matrix index
			unsigned 		Tex5MatIdx : 6;			// Tex5 matrix index
			unsigned 		Tex6MatIdx : 6;			// Tex6 matrix index
			unsigned 		Tex7MatIdx : 6;			// Tex7 matrix index
		};
		uint32_t bits;
	};

	static_assert (sizeof(MatrixIndex1) == sizeof(uint32_t), "MatrixIndex1 invalid definition!");

	// Texgen inrow enum
	enum TexGenInrow : unsigned
	{
		XF_TEXGEN_INROW_POSMTX = 0,
		XF_TEXGEN_INROW_NORMAL,
		XF_TEXGEN_INROW_COLORS,
		XF_TEXGEN_INROW_BINORMAL_T,
		XF_TEXGEN_INROW_BINORMAL_B,
		XF_TEXGEN_INROW_TEX0,
		XF_TEXGEN_INROW_TEX1,
		XF_TEXGEN_INROW_TEX2,
		XF_TEXGEN_INROW_TEX3,
		XF_TEXGEN_INROW_TEX4,
		XF_TEXGEN_INROW_TEX5,
		XF_TEXGEN_INROW_TEX6,
		XF_TEXGEN_INROW_TEX7
	};

	union TexGenParam
	{
		struct
		{
			unsigned    Reserved : 1;
			unsigned    projection : 1;		// texture projection 0: (s,t): texmul is 2x4 1: (s,t,q): texmul is 3x4
			unsigned    in_form : 1;		// input form (format of source input data for regular textures) 0: (A, B, 1.0, 1.0) (used for regular texture source) 1: (A, B, C, 1.0) (used for geometry or normal source)
			unsigned	Reserved2 : 1;
			unsigned    type : 3;			// texgen type 0: Regular transformation (transform incoming data), 1: texgen bump mapping, 2: Color texgen: (s,t)=(r,g:b) (g and b are concatenated), color0, 3: Color texgen: (s,t)=(r,g:b) (g and b are concatenated), color 1
			unsigned	src_row : 5;		// regular texture source row (TexGenInrow)
			unsigned    bump_src : 3;		// bump mapping source texture: n: use regular transformed tex(n) for bump mapping source
			unsigned    bump_light : 3;		// Bump mapping source light: n: use light #n for bump map direction source (10 to 17)
		};
		uint32_t bits;
	};

	static_assert (sizeof(TexGenParam) == sizeof(uint32_t), "TexGenParam invalid definition!");

	union DualGenParam
	{
		struct
		{
			unsigned    dualidx : 6;    // Indicates which is the base row of the dual transform matrix for regular texture coordinate
			unsigned    unused : 2;
			unsigned    norm : 1;       // specifies if texture coordinate should be normalized before send transform
		};
		uint32_t bits;
	};

	static_assert (sizeof(DualGenParam) == sizeof(uint32_t), "DualGenParam invalid definition!");

	struct XFState
	{
		// Matrix memory

		float mvTexMtx[(size_t)XFRegister::XF_MATRIX_MEMORY_SIZE];				// 0x0000-0x00ff (IndexA)
		float nrmMtx[(size_t)XFRegister::XF_NORMAL_MATRIX_MEMORY_SIZE];			// 0x0400-0x045f  (IndexB)
		float dualTexMtx[(size_t)XFRegister::XF_DUALTEX_MATRIX_MEMORY_SIZE];		// 0x0500-0x05ff  (IndexC)
		Light light[8];			// 0x0600-0x067f  (IndexD)

		// Other registers

		ClipDisable clipDisable;		// 0x1005
		InVertexSpec vtxSpec;			// 0x1008
		uint32_t numColors;			// 0x1009. Specifies the number of colors to output: 0: No xform colors active, 1: Xform supplies 1 color (host supplied or computed), 2: Xform supplies 2 colors (host supplied or computed)
		Color ambient[2];		// 0x100a, 0x100b. 32b: RGBA (8b/comp) Ambient color0/1 specifications
		Color material[2];		// 0x100c, 0x100d. 32b: RGBA (8b/comp) global color0/1 material specifications
		ColorAlphaControl colorControl[2];		// 0x100e, 0x100f
		ColorAlphaControl alphaControl[2];		// 0x1010, 0x1011
		uint32_t dualTexTran;		// 0x1012, B[0]: When set(1), enables dual transform for all texture coordinates. When reset (0), disables dual texture transform feature [rev B]
		MatrixIndex0 matIdxA;		// 0x1018
		MatrixIndex1 matIdxB;		// 0x1019
		float viewportScale[3];				// 0x101a-0x101c. Viewport scale X,Y,Z
		float viewportOffset[3];			// 0x101d-0x101f. Viewport offset X,Y,Z
		float projectionParam[6];	// 0x1020-0x1025
		bool projectOrtho;		// 0x1026. If set selects orthographic otherwise non-orthographic
		uint32_t numTex;			// 0x103f. Number of active textures
		TexGenParam tex[8];			// 0x1040-0x1047
		DualGenParam dualTex[8];		// 0x1050-0x1057
	};

#pragma pack(pop)

}



// TODO: Old implementation, will be redone nicely.

// current vertex data
typedef struct _Vertex
{
	float       pos[3];         // x, y, z
	float       nrm[9];         // x, y, z, normalized to [0, 1]
	Color       col[2];         // 2 color / alpha (RGBA)
	float       tcoord[8][4];   // s, t for eight tex units, last two for texgen
} Vertex;

typedef struct
{
	float   out[4];
} TexGenOut;

extern  Color   rasca[2];
extern  TexGenOut   tgout[8];
