// frag.glsl
#version 450

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in flat uint fragBlockID; // A Global ID!

// A Texture Array bekötése
layout(binding = 1) uniform sampler2DArray blockTextures;

layout(location = 0) out vec4 outColor;

void main() {
    // Basic napfény irány vektor
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float diff = max(dot(fragNormal, lightDir), 0.2); // Alap ambient fény 0.2

    // UV koordináták kiszámítása (Triplanar-szerű gyors leképezés a normal alapján)
    vec2 uv;
    if (abs(fragNormal.y) > 0.5) {
        uv = fragPos.xz; // Y tengelyen néz (Fel/Le)
    } else if (abs(fragNormal.x) > 0.5) {
        uv = fragPos.yz; // X tengelyen néz (Jobb/Bal)
    } else {
        uv = fragPos.xy; // Z tengelyen néz (Előre/Hátra)
    }

    // A varázslat: X, Y a pixel koordináta, a Z komponens (fragBlockID) pedig a Retegek (Layer) indexe!
    vec4 texColor = texture(blockTextures, vec3(uv.x, uv.y, float(fragBlockID)));

    // Átlátszóság (Levegő) eldobása
    if (texColor.a < 0.1) {
        discard;
    }

    outColor = vec4(texColor.rgb * diff, texColor.a);
}