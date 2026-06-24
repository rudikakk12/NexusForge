#version 450

// Ezek a bemenetek, amiket a C++ Vertex struktúrából olvas fel
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in uint inPaletteID;

// Ezeket küldjük tovább a Fragment Shadernek
layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec3 fragColor;

// Push Constant (Ez a kamera "lencséje", ezen keresztül látjuk a 3D világot)
layout(push_constant) uniform PushConstants {
    mat4 render_matrix;
} push;

// Egy gyors, ideiglenes paletta a teszthez
vec3 getPaletteColor(uint id) {
    if (id == 1) return vec3(0.5, 0.3, 0.1); // Föld
    if (id == 2) return vec3(0.2, 0.8, 0.2); // Fű
    if (id == 3) return vec3(0.5, 0.5, 0.5); // Kő
    return vec3(0.8, 0.1, 0.8); // Ismeretlen blokk (Magenta)
}

void main() {
    // 3D pozíció beszorzása a kamerával
    gl_Position = push.render_matrix * vec4(inPosition, 1.0);

    fragNormal = inNormal;
    fragColor = getPaletteColor(inPaletteID);
}