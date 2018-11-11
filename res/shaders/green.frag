#version 440

// *********************************
// Define the output colour for the shader here

// *********************************

// Outgoing colour for the shader
layout(location = 0) out vec4 out_colour;
void main()
{
    out_colour = vec4(0.0, 1.0, 0.0, 1.0);
}