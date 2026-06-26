#version 450

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in flat uint fragBlockID;

layout(binding = 1) uniform sampler2DArray blockTextures;

layout(location = 0) out vec4 outColor;

void main() {
    // 1. DETEKTÍV TESZT: Ha a GPU 0-t kapott ID-ként (Levegő), fessük be VÖRÖSRE,
    // hogy lássuk a geometriát, ahelyett hogy eldobnánk a semmibe!
    if (fragBlockID == 0) {
        outColor = vec4(1.0, 0.0, 0.0, 1.0); // VÖRÖS: Adat-híd (Memória formátum) hiba a BasicMesher-ben!
        return;
    }

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

    vec4 texColor = texture(blockTextures, vec3(uv.x, uv.y, float(fragBlockID)));

    // 2. DETEKTÍV TESZT: Ha a textúra betöltő/fallback rossz volt és tényleg 0 az alpha
    if (texColor.a < 0.1) {
        outColor = vec4(1.0, 0.0, 1.0, 1.0); // MAGENTA: A textúra a VRAM-ban üres vagy átlátszó!
        return;
    }

    // Ha idáig eljutott, minden tökéletes:
    outColor = vec4(texColor.rgb * diff, 1.0);
}