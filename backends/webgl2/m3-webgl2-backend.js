/*
 * m3-webgl2-backend.js - Mini3 WebGL2 backend (browser reference implementation)
 *
 * Implements the same renderer contract as the native backends, but on top of
 * the browser's WebGL2. It is intentionally small and readable — the WebGL 2
 * mechanics themselves are the reference for writing other backends.
 *
 * Design:
 *   - flat shading: fragment-shader face normal via dFdx/dFdy (ES 3.00)
 *   - Gouraud shading: per-vertex lighting in the vertex shader
 *   - per-draw uniforms (model, viewProj, normal matrix, lights) uploaded
 *   - shaders are cached per shading mode and created lazily
 *
 * This backend is NOT wired to the C core inside this demo; it is written
 * against the same contract. The demo JS scene is the browser-side mirror of
 * the native demo. (A wasm build of the C core provides the Rust/C-native
 * equivalent — see docs/WEBGL2_DEMO.md.)
 */

export const SHADING = Object.freeze({ FLAT: 'flat', GOURAUD: 'gouraud' });

/* ================================================================== */
/* Shaders (GLSL ES 3.00)                                              */
/* ================================================================== */

const FLAT_VERTEX = `#version 300 es
in vec3 aPos;
in vec3 aNormal;
out vec3 vWorldPos;
uniform mat4 uModel;
uniform mat4 uViewProj;
void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    gl_Position = uViewProj * worldPos;
    vWorldPos = worldPos.xyz;
}`;

const FLAT_FRAGMENT = `#version 300 es
precision mediump float;
in vec3 vWorldPos;
uniform vec3 uDiffuse;
uniform vec3 uAmbient;
uniform vec3 uLightDir;
uniform vec3 uLightColor;
out vec4 outColor;
void main() {
    vec3 fdx = dFdx(vWorldPos);
    vec3 fdy = dFdy(vWorldPos);
    vec3 n = normalize(cross(fdx, fdy));
    float d = dot(n, uLightDir);
    vec3 I = uAmbient;
    if (d > 0.0) I += d * uLightColor;
    outColor = vec4(uDiffuse * I, 1.0);
}`;

const GOURAUD_VERTEX = `#version 300 es
in vec3 aPos;
in vec3 aNormal;
in vec3 aColor;
out vec3 vColor;
uniform mat4 uModel;
uniform mat4 uViewProj;
uniform mat4 uNormalMat;
uniform vec3 uDiffuse;
uniform vec3 uAmbient;
uniform vec3 uLightDir;
uniform vec3 uLightColor;
void main() {
    gl_Position = uViewProj * uModel * vec4(aPos, 1.0);
    vec3 worldNormal = normalize((uNormalMat * vec4(aNormal, 0.0)).xyz);
    float d = dot(worldNormal, uLightDir);
    vec3 I = uAmbient;
    if (d > 0.0) I += d * uLightColor;
    vColor = uDiffuse * I * aColor;
}`;

const GOURAUD_FRAGMENT = `#version 300 es
precision mediump float;
in vec3 vColor;
out vec4 outColor;
void main() {
    outColor = vec4(vColor, 1.0);
}`;

/* ================================================================== */
/* Mat4 helpers (column-major, financial: m[4*col + row])              */
/* ================================================================== */

function mat4Identity() {
    const m = new Float32Array(16);
    m[0] = 1; m[5] = 1; m[10] = 1; m[15] = 1;
    return m;
}

function mat4Multiply(a, b) {
    const out = new Float32Array(16);
    for (let c = 0; c < 4; c++) {
        for (let r = 0; r < 4; r++) {
            out[4 * c + r] =
                a[4 * 0 + r] * b[4 * c + 0] +
                a[4 * 1 + r] * b[4 * c + 1] +
                a[4 * 2 + r] * b[4 * c + 2] +
                a[4 * 3 + r] * b[4 * c + 3];
        }
    }
    return out;
}

function mat4Translation(tx, ty, tz) {
    const m = mat4Identity();
    m[12] = tx; m[13] = ty; m[14] = tz;
    return m;
}

function mat4Scale(sx, sy, sz) {
    const m = mat4Identity();
    m[0] = sx; m[5] = sy; m[10] = sz;
    return m;
}

function mat4RotationEuler(pitch, yaw, roll) {
    /* q = yaw(Y) * pitch(X) * roll(Z), row-major-spec implemented carefully
       to match the C engine. */
    const hy = Math.sin(yaw * 0.5), sy = Math.cos(yaw * 0.5);
    const hp = Math.sin(pitch * 0.5), sp = Math.cos(pitch * 0.5);
    const hr = Math.sin(roll * 0.5), sr = Math.cos(roll * 0.5);
    /* qx (pitch), qy (yaw), qz (roll) quaternion mul: q = qy * qx * qz */
    const qx = { x: hp, y: 0, z: 0, w: sp };
    const qy = { x: 0, y: hy, z: 0, w: sy };
    const qz = { x: 0, y: 0, z: hr, w: sr };
    const q1 = quatMul(qy, qx);
    const q = quatMul(q1, qz);
    return quatToMat4(q);
}

function quatMul(a, b) {
    return {
        x: b.x * a.w + a.x * b.w + b.y * a.z - a.y * b.z,
        y: a.y * b.w + b.y * a.w + a.z * b.x - b.z * a.x,
        z: a.z * b.w + b.z * a.w + a.x * b.y - b.x * a.y,
        w: a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
    };
}

function quatToMat4(q) {
    /* q normalized for safety */
    const len = Math.sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (len < 1e-12) return mat4Identity();
    const x = q.x / len, y = q.y / len, z = q.z / len, w = q.w / len;
    const m = new Float32Array(16);
    m[0] = 1 - 2 * (y * y + z * z);
    m[1] = 2 * (x * y + w * z);
    m[2] = 2 * (x * z - w * y);
    m[4] = 2 * (x * y - w * z);
    m[5] = 1 - 2 * (x * x + z * z);
    m[6] = 2 * (y * z + w * x);
    m[8] = 2 * (x * z + w * y);
    m[9] = 2 * (y * z - w * x);
    m[10] = 1 - 2 * (x * x + y * y);
    m[15] = 1;
    return m;
}

function mat4Perspective(fovDeg, aspect, near, far) {
    const f = 1.0 / Math.tan((fovDeg * Math.PI / 180.0) * 0.5);
    const m = new Float32Array(16);
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (far + near) / (near - far);
    m[11] = -1.0;
    m[14] = 2.0 * far * near / (near - far);
    return m;
}

function mat4LookAt(eye, target, up) {
    /* right-handed, looking down -z */
    let f = [target[0] - eye[0], target[1] - eye[1], target[2] - eye[2]];
    const fl = Math.hypot(f[0], f[1], f[2]);
    if (fl < 1e-12) { f = [0, 0, -1]; } else { f = [f[0] / fl, f[1] / fl, f[2] / fl]; }
    let r = [f[1] * up[2] - f[2] * up[1], f[2] * up[0] - f[0] * up[2], f[0] * up[1] - f[1] * up[0]];
    const rl = Math.hypot(r[0], r[1], r[2]);
    if (rl < 1e-12) r = [1, 0, 0]; else r = [r[0] / rl, r[1] / rl, r[2] / rl];
    const u = [r[1] * f[2] - r[2] * f[1], r[2] * f[0] - r[0] * f[2], r[0] * f[1] - r[1] * f[0]];

    const m = new Float32Array(16);
    m[0] = r[0]; m[1] = u[0]; m[2] = -f[0];
    m[4] = r[1]; m[5] = u[1]; m[6] = -f[1];
    m[8] = r[2]; m[9] = u[2]; m[10] = -f[2];
    m[12] = -(r[0] * eye[0] + r[1] * eye[1] + r[2] * eye[2]);
    m[13] = -(u[0] * eye[0] + u[1] * eye[1] + u[2] * eye[2]);
    m[14] = f[0] * eye[0] + f[1] * eye[1] + f[2] * eye[2];
    m[15] = 1;
    return m;
}

/* Upper 3x3 of a mat4 assumed to carry TRS; we only need the normal matrix,
 * which is transpose(inverse(upper 3x3)). */
function upper3x3(m) {
    /* row-major: [m[0], m[4], m[8]], [m[1], m[5], m[9]], [m[2], m[6], m[10]] */
    return [
        m[0], m[4], m[8],
        m[1], m[5], m[9],
        m[2], m[6], m[10],
    ];
}

function inverseTranspose3x3(rows) {
    const [a, b, c] = [rows[0], rows[1], rows[2]];
    const [d, e, f] = [rows[3], rows[4], rows[5]];
    const [g, h, i] = [rows[6], rows[7], rows[8]];
    const A = e * i - f * h;
    const B = c * h - b * i;
    const C = b * f - c * e;
    const D = f * g - d * i;
    const E = a * i - c * g;
    const F = c * d - a * f;
    const G = d * h - e * g;
    const H = b * g - a * h;
    const I = a * e - b * d;
    const det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
    if (Math.abs(det) < 1e-12) return upper3x3Identity();
    const inv = 1.0 / det;
    const invm = [
        A * inv, B * inv, C * inv,
        D * inv, E * inv, F * inv,
        G * inv, H * inv, I * inv,
    ];
    /* normal matrix = transpose of inverse */
    return [
        invm[0], invm[3], invm[6],
        invm[1], invm[4], invm[7],
        invm[2], invm[5], invm[8],
    ];
}

function upper3x3Identity() {
    return [1, 0, 0, 0, 1, 0, 0, 0, 1];
}

/* ================================================================== */
/* Backend                                                             */
/* ================================================================== */

export class WebGL2Backend {
    constructor(gl) {
        if (!gl || typeof gl.getExtension !== 'function') {
            throw new Error('WebGL2Backend requires a WebGL2 render context');
        }
        if (!gl.getParameter) throw new Error('Not a WebGL context');
        this.gl = gl;
        this.viewport = { x: 0, y: 0, width: 0, height: 0 };
        this.clearColor = [0.03, 0.03, 0.04, 1.0];
        this._shaders = {};
        this._geometries = new Set();
        this._vaoCounter = 0;
        this._currentProgram = null;
    }

    /* --- contract: viewport --- */
    setViewport(x, y, width, height) {
        this.viewport = { x, y, width, height };
        this.gl.viewport(x, y, width, height);
        return true;
    }

    /* --- contract: geometry --- */
    createGeometry(mesh) {
        const gl = this.gl;
        if (!mesh || !mesh.positions || !mesh.indices || mesh.vertexCount === 0) return null;

        const vertexCount = mesh.vertexCount;
        const positions = mesh.positions instanceof Float32Array ? mesh.positions : new Float32Array(mesh.positions);
        let normals;
        if (mesh.normals instanceof Float32Array) normals = mesh.normals;
        else if (mesh.normals) normals = new Float32Array(mesh.normals);
        else {
            normals = new Float32Array(vertexCount * 3);
            for (let i = 0; i < vertexCount; i++) {
                normals[i * 3 + 0] = 0; normals[i * 3 + 1] = 1; normals[i * 3 + 2] = 0;
            }
        }
        let colors;
        if (mesh.colors instanceof Float32Array) colors = mesh.colors;
        else if (mesh.colors) colors = new Float32Array(mesh.colors);
        else {
            colors = new Float32Array(vertexCount * 3).fill(1);
        }
        const indices = mesh.indices instanceof Uint32Array ? mesh.indices : new Uint32Array(mesh.indices);

        const vao = gl.createVertexArray();
        const bufPos = gl.createBuffer();
        const bufNorm = gl.createBuffer();
        const bufColor = gl.createBuffer();
        const bufIndex = gl.createBuffer();

        gl.bindVertexArray(vao);

        gl.bindBuffer(gl.ARRAY_BUFFER, bufPos);
        gl.bufferData(gl.ARRAY_BUFFER, positions, gl.STATIC_DRAW);
        gl.enableVertexArrayAttrib(vao, 0);
        gl.vertexAttribPointer(0, 3, gl.FLOAT, false, 0, 0);

        gl.bindBuffer(gl.ARRAY_BUFFER, bufNorm);
        gl.bufferData(gl.ARRAY_BUFFER, normals, gl.STATIC_DRAW);
        gl.enableVertexArrayAttrib(vao, 1);
        gl.vertexAttribPointer(1, 3, gl.FLOAT, false, 0, 0);

        gl.bindBuffer(gl.ARRAY_BUFFER, bufColor);
        gl.bufferData(gl.ARRAY_BUFFER, colors, gl.STATIC_DRAW);
        gl.enableVertexArrayAttrib(vao, 2);
        gl.vertexAttribPointer(2, 3, gl.FLOAT, false, 0, 0);

        gl.bindVertexArray(null);

        gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, bufIndex);
        gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, indices, gl.STATIC_DRAW);
        gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, null);

        const geometry = {
            vao,
            buffers: [bufPos, bufNorm, bufColor, bufIndex],
            vertexCount: mesh.vertexCount,
            indexCount: mesh.indexCount,
            indexType: gl.UNSIGNED_INT,
        };
        this._geometries.add(geometry);
        return geometry;
    }

    destroyGeometry(geometry) {
        if (!geometry) return;
        const gl = this.gl;
        for (const buf of geometry.buffers) gl.deleteBuffer(buf);
        gl.deleteVertexArray(geometry.vao);
        this._geometries.delete(geometry);
    }

    /* --- contract: shaders --- */
    createShader(mode) {
        if (mode !== SHADING.FLAT && mode !== SHADING.GOURAUD) return null;
        if (this._shaders[mode]) return this._shaders[mode];

        const gl = this.gl;
        const isFlat = mode === SHADING.FLAT;
        const vs = isFlat ? FLAT_VERTEX : GOURAUD_VERTEX;
        const fs = isFlat ? FLAT_FRAGMENT : GOURAUD_FRAGMENT;

        const vsSrc = vs;
        const fsSrc = fs;

        const vsObj = gl.createShader(gl.VERTEX_SHADER);
        if (!vsObj) return null;
        gl.shaderSource(vsObj, vsSrc);
        gl.compileShader(vsObj);
        if (!this._isCompiled(vsObj)) { this._logShaderError('VERTEX_SHADER', vsObj, gl); return null; }

        const fsObj = gl.createShader(gl.FRAGMENT_SHADER);
        if (!fsObj) { gl.deleteShader(vsObj); return null; }
        gl.shaderSource(fsObj, fsSrc);
        gl.compileShader(fsObj);
        if (!this._isCompiled(fsObj)) { this._logShaderError('FRAGMENT_SHADER', fsObj, gl); gl.deleteShader(vsObj); return null; }

        const program = gl.createProgram();
        if (!program) { gl.deleteShader(vsObj); gl.deleteShader(fsObj); return null; }
        gl.attachShader(program, vsObj);
        gl.attachShader(program, fsObj);
        gl.linkProgram(program);
        gl.deleteShader(vsObj);
        gl.deleteShader(fsObj);

        if (!this._isLinked(program)) { return null; }

        const shader = {
            mode,
            program,
            uniforms: {
                uModel: gl.getUniformLocation(program, 'uModel'),
                uViewProj: gl.getUniformLocation(program, 'uViewProj'),
                uNormalMat: gl.getUniformLocation(program, 'uNormalMat'),
                uDiffuse: gl.getUniformLocation(program, 'uDiffuse'),
                uAmbient: gl.getUniformLocation(program, 'uAmbient'),
                uLightDir: gl.getUniformLocation(program, 'uLightDir'),
                uLightColor: gl.getUniformLocation(program, 'uLightColor'),
            },
        };
        this._shaders[mode] = shader;
        return shader;
    }

    _isCompiled(shaderObj) {
        try { return this.gl.getShaderParameter(shaderObj, this.gl.COMPILE_STATUS) === true; }
        catch (_) { return false; }
    }

    _logShaderError(kind, shaderObj, gl) {
        const info = gl.getShaderInfoLog(shaderObj);
        console.error(`[WebGL2Backend] ${kind} failed to compile: ${info}`);
    }

    _isLinked(program) {
        try { return this.gl.getProgramParameter(program, this.gl.LINK_STATUS) === true; }
        catch (_) { return false; }
    }

    destroyShader(shader) {
        if (!shader) return;
        this.gl.deleteProgram(shader.program);
        if (shader.mode && this._shaders[shader.mode] === shader) delete this._shaders[shader.mode];
    }

    /* --- contract: frame management --- */
    beginFrame(frame) {
        const gl = this.gl;
        const v = this.viewport;
        gl.viewport(v.x, v.y, v.width, v.height);
        gl.enable(gl.DEPTH_TEST);
        gl.depthFunc(gl.LESS);
        const c = this.clearColor;
        gl.clearColor(c[0], c[1], c[2], c[3]);
        gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
    }

    submit(frame, draw, geometry, shader) {
        if (!frame || !draw || !geometry || !shader) return false;
        const gl = this.gl;

        const model = draw.model || mat4Identity();
        const viewProj = mat4Multiply(frame.proj, frame.view);
        const normalMat3 = inverseTranspose3x3(upper3x3(model));
        /* normal matrix as mat4 (upper 3x3 + identity bottom-right) */
        const normalMat = [
            normalMat3[0], normalMat3[3], normalMat3[6], 0,
            normalMat3[1], normalMat3[4], normalMat3[7], 0,
            normalMat3[2], normalMat3[5], normalMat3[8], 0,
            0, 0, 0, 1,
        ];

        const u = shader.uniforms || {};
        gl.useProgram(shader.program);
        this._currentProgram = shader.program;
        gl.bindVertexArray(geometry.vao);
        gl.uniformMatrix4fv(u.uViewProj, false, viewProj);
        gl.uniformMatrix4fv(u.uModel, false, model);
        if (shader.mode === SHADING.GOURAUD) {
            gl.uniformMatrix4fv(u.uNormalMat, false, normalMat);
        }
        const diffuse = draw.material && draw.material.diffuse ? draw.material.diffuse : { x: 1, y: 1, z: 1 };
        gl.uniform3f(u.uDiffuse, diffuse.x ?? diffuse[0] ?? 1, diffuse.y ?? diffuse[1] ?? 1, diffuse.z ?? diffuse[2] ?? 1);

        const lights = frame.lights;
        const ambient = lights ? lights.ambient : { x: 0.2, y: 0.2, z: 0.25 };
        const ambientVec = this._vec3(ambient);
        gl.uniform3f(u.uAmbient, ambientVec[0], ambientVec[1], ambientVec[2]);

        /* directional light: take the first directional light in the frame */
        let dir = [0.4, 1.0, 0.6], col = [1.0, 0.95, 0.9];
        if (lights && lights.directional && lights.directional.length > 0) {
            const d = this._vec3(lights.directional[0].direction);
            const len = Math.hypot(d[0], d[1], d[2]) || 1;
            dir = [d[0] / len, d[1] / len, d[2] / len];
            const c = this._vec3(lights.directional[0].color);
            col = [c[0], c[1], c[2]];
        }
        gl.uniform3f(u.uLightDir, dir[0], dir[1], dir[2]);
        gl.uniform3f(u.uLightColor, col[0], col[1], col[2]);

        gl.drawElements(gl.TRIANGLES, geometry.indexCount, geometry.indexType, 0);

        gl.bindVertexArray(null);
        return true;
    }

    _vec3(v) {
        if (!v) return [0, 1, 0];
        return [v.x ?? v[0] ?? 0, v.y ?? v[1] ?? 0, v.z ?? v[2] ?? 0];
    }

    endFrame(frame) {
        /* In a browser, the current draw is presented by the compositor each
           frame — nothing to do here; the frame's job is done via begin/submit. */
        void frame;
    }

    destroy() {
        for (const shader of Object.values(this._shaders || {})) {
            this.gl.deleteProgram(shader.program);
        }
        this._shaders = {};
        for (const geometry of [...this._geometries]) {
            this.destroyGeometry(geometry);
        }
        this._currentProgram = null;
    }
}

/* ================================================================== */
/* Convenience: build a mesh description                               */
/* ================================================================== */

export function makeCubeMesh(halfExtent) {
    const h0 = -halfExtent, h1 = halfExtent;
    const size = halfExtent * 2;
    /* 24-vert cube (per-face split), white colors, face normals */
    const corners = [];
    const normals = [];
    const colors = [];
    const faces = [
        { n: [1, 0, 0], corners: [[h1, h0, h0], [h1, h1, h0], [h1, h1, h1], [h1, h0, h1]] },
        { n: [-1, 0, 0], corners: [[h0, h0, h1], [h0, h1, h1], [h0, h1, h0], [h0, h0, h0]] },
        { n: [0, 1, 0], corners: [[h0, h1, h0], [h0, h1, h1], [h1, h1, h1], [h1, h1, h0]] },
        { n: [0, -1, 0], corners: [[h0, h0, h1], [h0, h0, h0], [h1, h0, h0], [h1, h0, h1]] },
        { n: [0, 0, 1], corners: [[h0, h0, h1], [h1, h0, h1], [h1, h1, h1], [h0, h1, h1]] },
        { n: [0, 0, -1], corners: [[h1, h0, h0], [h0, h0, h0], [h0, h1, h0], [h1, h1, h0]] },
    ];
    for (const face of faces) {
        for (const c of face.corners) {
            corners.push(c[0], c[1], c[2]);
            normals.push(face.n[0], face.n[1], face.n[2]);
            colors.push(1, 1, 1);
        }
    }
    const indices = [];
    for (let f = 0; f < 6; f++) {
        const base = f * 4;
        indices.push(base + 0, base + 1, base + 2, base + 0, base + 2, base + 3);
    }
    return {
        positions: corners,
        normals,
        colors,
        indices,
        vertexCount: corners.length / 3,
        indexCount: indices.length,
        size,
    };
}
