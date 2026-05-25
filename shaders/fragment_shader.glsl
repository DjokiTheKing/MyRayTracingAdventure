#version 460 core
out vec4 FragColor;
	
in vec2 TexCoords;
	
uniform sampler2D tex;

vec3 acesFilm(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{             
    vec3 texCol = texture(tex, TexCoords).rgb;    
	float gamma = 2.2;
    FragColor = vec4(pow(acesFilm(texCol), vec3(1.0/gamma)), 1.0);
}