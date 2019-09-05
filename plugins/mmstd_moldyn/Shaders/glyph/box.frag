in vec4 objPos;
in vec4 camPos;
in vec4 lightPos;
in vec4 vertColor;

in vec3 invRad;

in mat3 rotMat;
in vec3 rotMatT0;
in vec3 rotMatT1; // rotation matrix from the quaternion
in vec3 rotMatT2;
in mat3 rotMatIT;

in flat vec3 dirColor;

in flat vec3 normal;
in flat vec3 transformedNormal;

layout (location = 0) out vec4 out_frag_color;

void main() {
    //vec3 normal = vec3(1,0,0);
    //normal = rotMatIT * normal;

    vec3 color = mix(dirColor, vertColor.rgb, colorInterpolation);

    vec3 ray = objPos.xyz - camPos.xyz;
    out_frag_color = vec4(color, 1.0);
    //out_frag_color = vec4(LocalLighting(ray, transformedNormal, lightPos.xyz, color), 1.0);
}