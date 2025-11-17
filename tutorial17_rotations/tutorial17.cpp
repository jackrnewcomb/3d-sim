#include "common/objloader.hpp"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include "common/controls.hpp"
#include "common/shader.hpp"  // LoadShaders from tutorial
#include "common/texture.hpp" // loadBMP_custom
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
GLFWwindow *window = nullptr; // define the global

int main(void)
{
    // Initialize GLFW
    if (!glfwInit())
    {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return -1;
    }

    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(800, 600, "BMP Texture Rectangle", NULL, NULL);
    if (!window)
    {
        fprintf(stderr, "Failed to open GLFW window\n");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    glewExperimental = true;
    if (glewInit() != GLEW_OK)
    {
        fprintf(stderr, "Failed to initialize GLEW\n");
        return -1;
    }

    // after glewInit()
    int fbWidth, fbHeight;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    // set a visible clear color so white screen is obvious
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    GLfloat vertices[] = {
        // Positions        // UVs
        -1.0f, -0.5f, 0.0f, 0.0f, 0.0f, // Bottom-left
        1.0f,  -0.5f, 0.0f, 1.0f, 0.0f, // Bottom-right
        1.0f,  0.5f,  0.0f, 1.0f, 1.0f, // Top-right
        -1.0f, 0.5f,  0.0f, 0.0f, 1.0f  // Top-left
    };

    GLuint indices[] = {0, 1, 2, 2, 3, 0};

    GLuint VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Load shaders (use your tutorial shader files)
    GLuint programID = LoadShaders("StandardShading.vertexshader", "StandardShading.fragmentshader");
    // immediate check after LoadShaders(...)
    if (programID == 0)
    {
        fprintf(stderr, "Shader program failed to load (programID == 0)\n");
        return -1;
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // Load image
    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true); // Flip BMP vertically
    unsigned char *data = stbi_load("ff.bmp", &width, &height, &nrChannels, 0);
    if (data)
    {
        printf("Loaded texture: %d x %d, channels=%d\n", width, height, nrChannels);
    }
    else
    {
        fprintf(stderr, "Failed to load texture (data == NULL)\n");
    }

    /*
    Load and handle OBJ
    */
    std::vector<float> verts, uvs, norms;

    if (!loadOBJ("chicken_01.obj", verts, uvs, norms))
    {
        printf("OBJ load failed!\n");
    }

    GLuint objVAO, objVBO;
    glGenVertexArrays(1, &objVAO);
    glGenBuffers(1, &objVBO);

    glBindVertexArray(objVAO);

    glBindBuffer(GL_ARRAY_BUFFER, objVBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    if (data)
    {
        GLenum format = (nrChannels == 3) ? GL_RGB : GL_RGBA;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        // Texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    else
    {
        fprintf(stderr, "Failed to load texture\n");
    }
    stbi_image_free(data);

    glUseProgram(programID);
    glUniform1i(glGetUniformLocation(programID, "myTextureSampler"), 0);

    // Camera settings
    glm::vec3 cameraPos = glm::vec3(0.0f, 0.5f, 5.0f);
    glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

    float yaw = -90.0f; // horizontal rotation
    float pitch = 0.0f; // vertical rotation

    float lastX = 400, lastY = 300; // center of window
    bool firstMouse = true;

    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

    float cameraSpeed = 2.5f; // units per second

    // --- BEFORE the main loop ---
    GLuint MatrixID = glGetUniformLocation(programID, "MVP");

    // Optional: precompute a base field VAO scale if you want
    glm::vec3 fieldScale = glm::vec3(5.0f, 0.01f, 3.0f); // wide, thin “floor”

    // Main render loop
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        float velocity = cameraSpeed * deltaTime;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            cameraPos += velocity * cameraFront;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            cameraPos -= velocity * cameraFront;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * velocity;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * velocity;

        // Clear buffers
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Use shader program
        glUseProgram(programID);

        // --- Compute camera matrices ---
        // --- Compute View & Projection ---
        glm::mat4 View = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glm::mat4 Projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);
        // --- Draw football field ---
        glm::vec3 fieldScale = glm::vec3(5.0f, 1.0f, 3.0f); // wider X/Z, taller Y
        glm::mat4 fieldModel = glm::mat4(1.0f);
        fieldModel = glm::translate(fieldModel, glm::vec3(0.0f, -0.5f, 0.0f)); // move down so top aligns with y=0
        fieldModel = glm::scale(fieldModel, fieldScale);

        glm::mat4 fieldMVP = Projection * View * fieldModel;
        glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &fieldMVP[0][0]);

        glUniform1i(glGetUniformLocation(programID, "useSolidColor"), 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // --- Draw chicken OBJ ---
        glm::mat4 chickenModel = glm::mat4(1.0f);
        chickenModel = glm::translate(chickenModel, glm::vec3(0.0f, 0.0f, 0.0f));
        chickenModel = glm::scale(chickenModel, glm::vec3(0.01f)); // scale down large OBJ
        chickenModel = glm::rotate(chickenModel, glm::radians(180.0f), glm::vec3(0, 1, 0));

        glm::mat4 chickenMVP = Projection * View * chickenModel;
        glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &chickenMVP[0][0]);

        glUniform1i(glGetUniformLocation(programID, "useSolidColor"), 1);
        glUniform3f(glGetUniformLocation(programID, "solidColor"), 0.0f, 0.0f, 0.0f);

        glBindVertexArray(objVAO);
        glDrawArrays(GL_TRIANGLES, 0, verts.size() / 3);

        // Swap buffers and poll events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);

    glfwTerminate();
    return 0;
}
