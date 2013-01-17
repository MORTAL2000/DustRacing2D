// This file is part of Dust Racing (DustRAC).
// Copyright (C) 2012 Jussi Lind <jussi.lind@iki.fi>
//
// DustRAC is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// DustRAC is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with DustRAC. If not, see <http://www.gnu.org/licenses/>.

#version 130

in vec3 inVertex;
in vec3 inNormal;
in vec2 inTexCoord;
in vec4 inColor;

uniform vec4 pos;
uniform vec4 scale;
uniform vec2 angle;
uniform vec4 color;
uniform mat4 mvp;

uniform float fade;

uniform vec4 diffuseLightDir;
uniform vec4 diffuseLightColor;

out vec2 texCoord0;
out vec4 vColor;

void main()
{
    float sin1 = angle.x;
    float cos1 = angle.y;

    mat4 translation = mat4(
        cos1,  sin1,  0.0,   0.0,
        -sin1, cos1,  0.0,   0.0,
        0.0,   0.0,   1.0,   0.0,
        pos.x, pos.y, pos.z, 1.0);

    // Normal MVP transform
    gl_Position = mvp * translation * (vec4(inVertex, 1) * scale);

    float diffuseLightIntensity = dot(normalize(diffuseLightDir), vec4(-inNormal, 1));
    
    // Copy the primary color
    vec4 ambientLight = vec4(1.0, 0.95, 0.9, 1.0);
    vColor = mix(inColor * color, ambientLight * diffuseLightColor * diffuseLightIntensity, 0.5) * fade;

    // Copy texture coorinates
    texCoord0 = inTexCoord;
}

