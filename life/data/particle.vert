#version 330 core

#define ARRAY_SIZE 100

layout(location = 0) in vec3 vertex;

uniform vec3 pos[ARRAY_SIZE];
uniform vec3 size[ARRAY_SIZE];
uniform vec4 color[ARRAY_SIZE];

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

    modelMatrix *= BuildTranslation(pos[gl_InstanceID]);
    modelMatrix *= BuildScale(size[gl_InstanceID]);

    gl_Position = projectionMatrix * modelMatrix * vec4(vertex, 1.0);
    fragColor = color[gl_InstanceID];
}
