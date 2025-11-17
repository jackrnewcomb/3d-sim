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

float lastX = 400, lastY = 300; // center of window
bool firstMouse = true;
float yaw = -90.0f; // horizontal rotation
float pitch = 0.0f; // vertical rotation
float sensitivity = 0.1f;
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

void mouse_callback(GLFWwindow *window, double xpos, double ypos)
{
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed: y-coordinates go bottom -> top

    lastX = xpos;
    lastY = ypos;

    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

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

    // Scale coordinates to match field quad
    // --- Field vertex data ---
    float fieldWidth = 10.0f;  // X-axis width
    float fieldLength = 50.0f; // Z-axis length
    GLfloat fieldVertices[] = {
        // Positions                     // UVs
        -fieldWidth / 2, 0.0f, -fieldLength / 2, 0.0f, 0.0f, // Bottom-left
        fieldWidth / 2,  0.0f, -fieldLength / 2, 1.0f, 0.0f, // Bottom-right
        fieldWidth / 2,  0.0f, fieldLength / 2,  1.0f, 1.0f, // Top-right
        -fieldWidth / 2, 0.0f, fieldLength / 2,  0.0f, 1.0f  // Top-left
    };
    GLuint fieldIndices[] = {0, 1, 2, 2, 3, 0};

    // --- Field VAO/VBO/EBO ---
    GLuint fieldVAO, fieldVBO, fieldEBO;
    glGenVertexArrays(1, &fieldVAO);
    glGenBuffers(1, &fieldVBO);
    glGenBuffers(1, &fieldEBO);

    glBindVertexArray(fieldVAO);

    glBindBuffer(GL_ARRAY_BUFFER, fieldVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(fieldVertices), fieldVertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, fieldEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(fieldIndices), fieldIndices, GL_STATIC_DRAW);

    // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    // UV
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    // Load shaders (use your tutorial shader files)
    GLuint programID = LoadShaders("StandardShading.vertexshader", "StandardShading.fragmentshader");
    // immediate check after LoadShaders(...)
    if (programID == 0)
    {
        fprintf(stderr, "Shader program failed to load (programID == 0)\n");
        return -1;
    }

    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // hide & capture cursor

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

    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

    float cameraSpeed = 2.5f; // units per second

    // --- BEFORE the main loop ---
    GLuint MatrixID = glGetUniformLocation(programID, "MVP");

    // Optional: precompute a base field VAO scale if you want
    glm::vec3 fieldScale = glm::vec3(5.0f, 0.01f, 3.0f); // wide, thin “floor”

    // Yard lines at 0, 25, 50, 25, 0 (like a V formation)
    std::vector<float> yardLines = {0.0f, 25.0f, 50.0f, 25.0f, 0.0f};

    // UAV positions container
    std::vector<glm::vec3> UAVPositions;

    // Map yard lines to Z coordinates
    for (float yard : yardLines)
    {
        float zPos = (yard / 50.0f) * (fieldLength / 2.0f);                // scale to field
        UAVPositions.push_back(glm::vec3(-fieldWidth / 2.0f, 0.0f, zPos)); // left
        UAVPositions.push_back(glm::vec3(0.0f, 0.0f, zPos));               // middle
        UAVPositions.push_back(glm::vec3(fieldWidth / 2.0f, 0.0f, zPos));  // right
    }

    // Main render loop
    while (!glfwWindowShouldClose(window))
    {
        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        cameraFront = glm::normalize(front);

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
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        // Clear buffers
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Use shader program
        glUseProgram(programID);

        // --- Compute camera matrices ---
        // --- Compute View & Projection ---
        glm::mat4 View = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        glm::mat4 Projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);
        // --- Draw football field ---
        glm::mat4 fieldModel = glm::mat4(1.0f);
        fieldModel = glm::translate(fieldModel, glm::vec3(0.0f, -0.01f, 0.0f)); // slightly below UAVs
        glm::mat4 fieldMVP = Projection * View * fieldModel;
        glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &fieldMVP[0][0]);

        glUniform1i(glGetUniformLocation(programID, "useSolidColor"), 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);

        glBindVertexArray(fieldVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // --- Draw chicken OBJ ---
        for (auto &pos : UAVPositions)
        {
            glm::mat4 Model = glm::mat4(1.0f);
            Model = glm::translate(Model, pos);
            Model = glm::scale(Model, glm::vec3(0.01f)); // scale down chicken
            Model = glm::rotate(Model, glm::radians(180.0f), glm::vec3(0, 1, 0));

            glm::mat4 MVP = Projection * View * Model;
            glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);

            glBindVertexArray(objVAO);
            glUniform1i(glGetUniformLocation(programID, "useSolidColor"), 1);
            glUniform3f(glGetUniformLocation(programID, "solidColor"), 0.0f, 0.0f, 0.0f);

            glDrawArrays(GL_TRIANGLES, 0, verts.size() / 3);
        }

        // Swap buffers and poll events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Delete field buffers
    glDeleteVertexArrays(1, &fieldVAO);
    glDeleteBuffers(1, &fieldVBO);
    glDeleteBuffers(1, &fieldEBO);

    // Delete chicken OBJ buffers
    glDeleteVertexArrays(1, &objVAO);
    glDeleteBuffers(1, &objVBO);

    glfwTerminate();
    return 0;
}
