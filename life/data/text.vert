#version 330 core

layout(location = 0) in vec3 position;

uniform vec3 size;
uniform vec3 pos;

uniform vec4 color;

uniform mat4 projectionMatrix;

out vec4 fragColor;

mat4 BuildTranslation(vec3 delta)
{
    return mat4(
        vec4(1.0, 0.0, 0.0, 0.0),
        vec4(0.0, 1.0, 0.0, 0.0),
        vec4(0.0, 0.0, 1.0, 0.0),
        vec4(delta, 1.0)
    );
}

mat4 BuildScale(vec3 scale)
{
    return mat4(
        vec4(scale.x, 0.0, 0.0, 0.0),
        vec4(0.0, scale.y, 0.0, 0.0),
        vec4(0.0, 0.0, scale.z, 0.0),
        vec4(0.0, 0.0, 0.0, 1.0)
    );
}

void main()
{
    mat4 modelMatrix = mat4(1.0);

    modelMatrix *= BuildTranslation(pos);
    modelMatrix *= BuildScale(size);

    gl_Position = projectionMatrix * modelMatrix * vec4(position, 1.0);
    fragColor = color;
}
