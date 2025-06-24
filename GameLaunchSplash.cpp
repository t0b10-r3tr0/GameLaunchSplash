#include <SDL2/SDL.h>
#include <GLES2/gl2.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <iostream>
#include <chrono>
#include <cmath>
#include <dirent.h>
#include <cstring>

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

bool isProcessRunning(const char* name) {
    DIR* dir = opendir("/proc");
    if (!dir) return false;
    struct dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (!isdigit(ent->d_name[0])) continue;
        char cmdline[512];
        snprintf(cmdline, sizeof(cmdline), "/proc/%s/cmdline", ent->d_name);
        FILE* f = fopen(cmdline, "r");
        if (f) {
            char buf[256];
            if (fgets(buf, sizeof(buf), f)) {
                if (strstr(buf, name)) {
                    fclose(f);
                    closedir(dir);
                    return true;
                }
            }
            fclose(f);
        }
    }
    closedir(dir);
    return false;
}

int main(int argc, char *argv[])
{
    // Accept 6 or 7 arguments (7th is optional background image)
    if (argc != 7 && argc != 8)
    {
        std::cerr << "Usage: " << argv[0] << " <image.png> <initial_scale> <final_scale> <fade_time> <show_time> <process_path> [background.png]\n";
        return -1;
    }

    const char *imagePath = argv[1];
    float initialScale = atof(argv[2]);
    float finalScale = atof(argv[3]);
    float fadeTime = atof(argv[4]);
    float showTime = atof(argv[5]);
    const char *processPath = argv[6];

    const char *bgImagePath = nullptr;
    if (argc == 8)
        bgImagePath = argv[7];

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

    GLuint bgTexture = 0;
    int bgW = 0, bgH = 0;
    if (bgImagePath) {
        int bgChannels;
        unsigned char *bgData = stbi_load(bgImagePath, &bgW, &bgH, &bgChannels, STBI_rgb_alpha);
        if (bgData) {
            glGenTextures(1, &bgTexture);
            glBindTexture(GL_TEXTURE_2D, bgTexture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bgW, bgH, 0, GL_RGBA, GL_UNSIGNED_BYTE, bgData);
            stbi_image_free(bgData);
        }
    }

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

    // Background scale (cover fit)
    float bgScale = 1.0f;
    if (bgTexture) {
        float scaleW = (float)screenW / bgW;
        float scaleH = (float)screenH / bgH;
        bgScale = std::max(scaleW, scaleH); // cover fit
    }

    auto start = std::chrono::high_resolution_clock::now();
    float total = fadeTime * 2 + showTime;

    SDL_Event e;
    bool running = true;
    const char* processName = argv[6];
    bool forceFadeOut = false;
    float fadeOutStart = 0.0f;

    while (running)
    {
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE))
                running = false;
        }

        auto now = std::chrono::high_resolution_clock::now();
        float elapsed = std::chrono::duration<float>(now - start).count();

        // Allow the fade-out phase to finish before breaking
        if (elapsed >= total + fadeTime)
            break;

        if (processName && !forceFadeOut) {
            if (isProcessRunning(processName)) {
                // Start fade out immediately
                forceFadeOut = true;
                // Set elapsed so that we jump to the fade-out phase
                fadeOutStart = elapsed;
            }
        }

        // Animation phases:
        // [0, fadeTime): background fade in
        // [fadeTime, 2*fadeTime): logo fade/scale in (bg fully visible)
        // [2*fadeTime, 2*fadeTime+showTime): both fully visible
        // [2*fadeTime+showTime, total): both fade out

        float bgAlpha = 1.0f;
        float logoAlpha = 1.0f, scale = baseScale;

        if (forceFadeOut && elapsed < fadeOutStart + fadeTime) {
            // During forced fade out
            float t = (elapsed - fadeOutStart) / fadeTime;
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            bgAlpha = 1.0f - t;
            logoAlpha = 1.0f - t;
            scale = lerp(baseScale, baseInitialScale, t);
        } else if (forceFadeOut && elapsed >= fadeOutStart + fadeTime) {
            // After forced fade out, exit
            break;
        } else if (elapsed < fadeTime) {
            // Background fade in
            bgAlpha = elapsed / fadeTime;
            logoAlpha = 0.0f;
            scale = baseInitialScale;
        } else if (elapsed < fadeTime * 2) {
            // Logo fade/scale in
            bgAlpha = 1.0f;
            float t = (elapsed - fadeTime) / fadeTime;
            logoAlpha = t;
            scale = lerp(baseInitialScale, baseScale, t);
        } else if (elapsed < fadeTime * 2 + showTime) {
            // Both fully visible
            bgAlpha = 1.0f;
            logoAlpha = 1.0f;
            scale = baseScale;
        } else {
            // Both fade out together
            float t = (elapsed - (fadeTime * 2 + showTime)) / fadeTime;
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            bgAlpha = 1.0f - t;
            logoAlpha = 1.0f - t;
            scale = lerp(baseScale, baseInitialScale, t);
        }

        glViewport(0, 0, screenW, screenH);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        // Draw background image (cover fit, fade in/out)
        if (bgTexture) {
            glBindTexture(GL_TEXTURE_2D, bgTexture);
            glUniform2f(uImg, (float)bgW, (float)bgH);
            glUniform1f(uScale, bgScale);
            glUniform1f(uAlpha, bgAlpha);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }

        // Draw main image (logo)
        glBindTexture(GL_TEXTURE_2D, texture);
        glUniform2f(uImg, (float)imgW, (float)imgH);
        glUniform1f(uScale, scale);
        glUniform1f(uAlpha, logoAlpha);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        SDL_GL_SwapWindow(window);
    }

    glDeleteBuffers(1, &vbo);
    if (bgTexture)
        glDeleteTextures(1, &bgTexture);
    glDeleteTextures(1, &texture);
    glDeleteProgram(shaderProgram);
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
