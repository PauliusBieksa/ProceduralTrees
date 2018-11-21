#version 410
layout (location = 0) in vec3 position;
layout (location = 1) in vec2 texCoord;
layout (location = 2) in vec3 normalIn;


uniform mat4 MVP;
uniform mat3 NM;


layout(location = 2) in vec3 normal;
layout(location = 2) out vec3 transformed_normal;

void main()
{
     gl_Position = MVP * vec4(position, 1.0f);  
	 transformed_normal = NM * normal;
}