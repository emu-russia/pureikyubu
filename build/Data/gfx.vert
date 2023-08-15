// Flipper GFX Engine vertex shader
#version 330 core

// Define the input attributes of the vertex passed through the VBO

in vec3 in_Position;
in vec3 in_Normal;
in vec3 in_Binormal;
in vec3 in_Tangent;
in uvec4 in_Color0;         // RGBA
in uvec4 in_Color1;
in vec2 in_TexCoord0;
in vec2 in_TexCoord1;
in vec2 in_TexCoord2;
in vec2 in_TexCoord3;
in vec2 in_TexCoord4;
in vec2 in_TexCoord5;
in vec2 in_TexCoord6;
in vec2 in_TexCoord7;
in uint MatrixIndex0;
in uint MatrixIndex1;

// Define uniforms to be updated when registers of the XF block are updated

uniform float matrix_mem[0x100];            // 0x0000-0x00ff (IndexA)
uniform float norm_matrix_mem[96];          // 0x0400-0x045f  (IndexB)
uniform float dual_tex_matrix_mem[0x100];   // 0x0500-0x05ff  (IndexC)

uniform struct Light
{
    uvec4 rgba;
    vec3 a;
    vec3 k;
    vec3 lpx;
    vec3 dhx;
} ligth_mem[8];                 // 0x0600-0x067f  (IndexD)

uniform uint vtx_spec;          // 0x1008
uniform uint num_colors;        // 0x1009
uniform uvec4 ambient[2];       // 0x100a, 0x100b
uniform uvec4 material[2];      // 0x100c, 0x100d
uniform uint color_control[2];      // 0x100e, 0x100f
uniform uint alpha_control[2];      // 0x1010, 0x1011
uniform uint dual_tex_tran;             // 0x1012
// matIdxA?
// matIdxB?
uniform float viewport_scale[3];    // 0x101a-0x101c
uniform float viewport_offset[3];   // 0x101d-0x101f
uniform float projection_param[6];      // 0x1020-0x1025
uniform uint projection_ortho;          // 0x1026
uniform uint num_tex;               // 0x103f
uniform uint texgen_param[8];       // 0x1040-0x1047
uniform uint dual_texgen_param[8];  // 0x1050-0x1057

// bitfieldExtract

void main(void) 
{
    gl_Position = vec4(in_Position.x, in_Position.y, 1.0, 1.0);
}
