#version 450

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in flat uint fragBlockID;

layout(binding = 1) uniform sampler2DArray blockTextures;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float diff = max(dot(fragNormal, lightDir), 0.2);

    vec2 uv;
    if (abs(fragNormal.y) > 0.5) {
        uv = fragPos.xz;
    } else if (abs(fragNormal.x) > 0.5) {
        uv = fragPos.zy;
    } else {
        uv = fragPos.xy;
    }

    // A réteg index explicit konverziója (a Garbage ID-k elkerülésére)
    float layerIndex = float(fragBlockID);
    vec4 texColor = texture(blockTextures, vec3(uv.x, uv.y, layerIndex));

    // HA A TEXTÚRA ÜRES VAGY AZ ID HIBÁS: Jöhet a Procedurális Lila-Fekete Sakktábla!
    if (texColor.a < 0.1) {
        // 4x4-es sűrű rács generálása a textúra UV koordinátáiból
        bool isMagenta = (int(floor(uv.x * 4.0)) + int(floor(uv.y * 4.0))) % 2 == 0;
        vec3 fallbackColor = isMagenta ? vec3(1.0, 0.0, 1.0) : vec3(0.0, 0.0, 0.0);

        outColor = vec4(fallbackColor * diff, 1.0);
        return;
    }

    // Ha megvan a PNG a VRAM-ban, akkor kirajzolja azt!
    outColor = vec4(texColor.rgb * diff, 1.0);
}