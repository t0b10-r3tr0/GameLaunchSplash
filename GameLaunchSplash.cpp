// main.cpp - OpenGL ES 2.0 Splash with Fade + Scale using Shaders
#include <SDL2/SDL.h>
#include <GLES2/gl2.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <iostream>
#include <chrono>
#include <cmath>

const char *vertexShaderSource = R"(
attribute vec2 aPos;
attribute vec2 aTexCoord;

varying vec2 vTexCoord;

uniform vec2 uResolution;
uniform vec2 uImageSize;
uniform float uScale;

void main() {
    vec2 scaledSize = uImageSize * uScale;
    vec2 pos = aPos * scaledSize + (uResolution - scaledSize) / 2.0;
    vec2 clipSpace = ((pos / uResolution) * 2.0 - 1.0);
    gl_Position = vec4(clipSpace * vec2(1, -1), 0.0, 1.0);
    vTexCoord = aTexCoord;
}
)";

const char *fragmentShaderSource = R"(
precision mediump float;
varying vec2 vTexCoord;
uniform sampler2D uTexture;
uniform float uAlpha;
void main() {
    vec4 texColor = texture2D(uTexture, vTexCoord);
    gl_FragColor = vec4(texColor.rgb, texColor.a * uAlpha);
}
)";

GLuint compileShader(GLenum type, const char *source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char info[512];
        glGetShaderInfoLog(shader, 512, nullptr, info);
        std::cerr << "Shader Compilation Error: " << info << "\n";
    }
    return shader;
}

GLuint createShaderProgram()
{
    GLuint vs = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

float lerp(float a, float b, float t)
{
    return a + t * (b - a);
}

int main(int argc, char *argv[])
{
    if (argc != 6)
    {
        std::cerr << "Usage: " << argv[0] << " <image.png> <initial_scale> <final_scale> <fade_time> <show_time>\n";
        return -1;
    }

    const char *imagePath = argv[1];
    float initialScale = atof(argv[2]);
    float finalScale = atof(argv[3]);
    float fadeTime = atof(argv[4]);
    float showTime = atof(argv[5]);

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *window = SDL_CreateWindow("Logo Splash",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          0, 0, SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_OPENGL);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    SDL_GLContext context = SDL_GL_CreateContext(window);
    int screenW, screenH;
    SDL_GetWindowSize(window, &screenW, &screenH);

    // Load image
    int imgW, imgH, channels;
    unsigned char *data = stbi_load(imagePath, &imgW, &imgH, &channels, STBI_rgb_alpha);
    if (!data)
    {
        std::cerr << "Failed to load image.\n";
        return -1;
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imgW, imgH, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);

    GLuint shaderProgram = createShaderProgram();
    glUseProgram(shaderProgram);

    GLint aPos = glGetAttribLocation(shaderProgram, "aPos");
    GLint aTex = glGetAttribLocation(shaderProgram, "aTexCoord");
    GLint uRes = glGetUniformLocation(shaderProgram, "uResolution");
    GLint uImg = glGetUniformLocation(shaderProgram, "uImageSize");
    GLint uScale = glGetUniformLocation(shaderProgram, "uScale");
    GLint uAlpha = glGetUniformLocation(shaderProgram, "uAlpha");

    float vertices[] = {
        0, 0, 0, 0,
        1, 0, 1, 0,
        0, 1, 0, 1,
        1, 1, 1, 1};

    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(aPos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(aPos);
    glVertexAttribPointer(aTex, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
    glEnableVertexAttribArray(aTex);

    glUniform2f(uRes, (float)screenW, (float)screenH);
    glUniform2f(uImg, (float)imgW, (float)imgH);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Compute the maximum allowed width/height for the image
    float maxWidth = screenW * finalScale;
    float maxHeight = screenH * finalScale;

    // Compute the scale so the image fits within those bounds (contain fit)
    float baseScale = std::min(maxWidth / imgW, maxHeight / imgH);
    float baseInitialScale = std::min(maxWidth / imgW, maxHeight / imgH) * (initialScale / finalScale);

    // Clamp so the image never exceeds the window in either dimension
    float maxAllowedScaleW = (float)screenW / imgW;
    float maxAllowedScaleH = (float)screenH / imgH;
    float maxAllowedScale = (maxAllowedScaleW < maxAllowedScaleH) ? maxAllowedScaleW : maxAllowedScaleH;

    if (baseScale > maxAllowedScale)
        baseScale = maxAllowedScale;
    if (baseInitialScale > maxAllowedScale)
        baseInitialScale = maxAllowedScale;

    auto start = std::chrono::high_resolution_clock::now();
    float total = fadeTime * 2 + showTime;

    SDL_Event e;
    bool running = true;
    while (running)
    {
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE))
                running = false;
        }

        auto now = std::chrono::high_resolution_clock::now();
        float elapsed = std::chrono::duration<float>(now - start).count();
        if (elapsed >= total)
            break;

        float alpha = 1.0f, scale = baseScale;
        if (elapsed < fadeTime)
        {
            float t = elapsed / fadeTime;
            alpha = t;
            scale = lerp(baseInitialScale, baseScale, t);
        }
        else if (elapsed < fadeTime + showTime)
        {
            alpha = 1.0f;
            scale = baseScale;
        }
        else
        {
            float t = (elapsed - fadeTime - showTime) / fadeTime;
            alpha = 1.0f - t;
            scale = lerp(baseScale, baseInitialScale, t);
        }

        glUniform1f(uAlpha, alpha);
        glUniform1f(uScale, scale);

        glViewport(0, 0, screenW, screenH);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        SDL_GL_SwapWindow(window);
    }

    glDeleteBuffers(1, &vbo);
    glDeleteTextures(1, &texture);
    glDeleteProgram(shaderProgram);
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
