#pragma once
#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace NF::Render {
    
    // 3D Vektor a mozgáshoz
    struct Vec3 {
        float x, y, z;
        Vec3 operator+(const Vec3& o) const { return {x+o.x, y+o.y, z+o.z}; }
        Vec3 operator-(const Vec3& o) const { return {x-o.x, y-o.y, z-o.z}; }
        Vec3 operator*(float s) const { return {x*s, y*s, z*s}; }
        Vec3 normalize() const {
            float len = std::sqrt(x*x + y*y + z*z);
            if(len == 0) return {0,0,0};
            return {x/len, y/len, z/len};
        }
        Vec3 cross(const Vec3& o) const {
            return {y*o.z - z*o.y, z*o.x - x*o.z, x*o.y - y*o.x};
        }
        float dot(const Vec3& o) const {
            return x*o.x + y*o.y + z*o.z;
        }
    };

    // 4x4-es Mátrix a Kamera forgatásához és a 3D illúzióhoz
    struct Mat4 {
        float m[4][4] = {{0}}; // Oszlop-major elrendezés (Ahogy a GPU szereti)
        
        static Mat4 identity() {
            Mat4 res;
            res.m[0][0] = 1; res.m[1][1] = 1; res.m[2][2] = 1; res.m[3][3] = 1;
            return res;
        }
        
        Mat4 operator*(const Mat4& o) const {
            Mat4 res;
            for(int c=0; c<4; ++c)
                for(int r=0; r<4; ++r)
                    res.m[c][r] = m[0][r]*o.m[c][0] + m[1][r]*o.m[c][1] + m[2][r]*o.m[c][2] + m[3][r]*o.m[c][3];
            return res;
        }
        
        static Mat4 lookAt(Vec3 eye, Vec3 center, Vec3 up) {
            Vec3 f = (center - eye).normalize();
            Vec3 s = f.cross(up).normalize();
            Vec3 u = s.cross(f);
            Mat4 res = identity();
            res.m[0][0] = s.x; res.m[1][0] = s.y; res.m[2][0] = s.z;
            res.m[0][1] = u.x; res.m[1][1] = u.y; res.m[2][1] = u.z;
            res.m[0][2] =-f.x; res.m[1][2] =-f.y; res.m[2][2] =-f.z;
            res.m[3][0] = -s.dot(eye);
            res.m[3][1] = -u.dot(eye);
            res.m[3][2] = f.dot(eye);
            return res;
        }
        
        static Mat4 perspective(float fovY, float aspect, float zNear, float zFar) {
            float tanHalfFovy = std::tan(fovY / 2.0f);
            Mat4 res;
            res.m[0][0] = 1.0f / (aspect * tanHalfFovy);
            res.m[1][1] = -1.0f / (tanHalfFovy); // A Vulkan fejjel lefelé van az OpenGL-hez képest, itt fordítjuk meg!
            res.m[2][2] = zFar / (zNear - zFar);
            res.m[2][3] = -1.0f;
            res.m[3][2] = -(zFar * zNear) / (zFar - zNear);
            return res;
        }
    };
}