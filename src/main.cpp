#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <mutex>
#include <vector>

#include "common/controls.hpp"
#include "common/objloader.hpp"
#include "common/shader.hpp"   // LoadShaders from tutorial
#include "common/texture.hpp"  // loadBMP_custom
#define STB_IMAGE_IMPLEMENTATION
#include "ECE_UAV.hpp"
#include "stb_image.h"

GLFWwindow* window = nullptr;  // define the global

float lastX = 400, lastY = 300;  // center of window
bool firstMouse = true;
float yaw = -90.0f;  // horizontal rotation
float pitch = 0.0f;  // vertical rotation
float sensitivity = 0.1f;
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

/**
 * @brief Provides handling for mouse input to adjust camera angle
 *
 * @param window
 * @param x position
 * @param y position
 */
void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
  if (firstMouse) {
    lastX = xpos;
    lastY = ypos;
    firstMouse = false;
  }

  float xoffset = xpos - lastX;
  float yoffset = lastY - ypos;

  lastX = xpos;
  lastY = ypos;

  xoffset *= sensitivity;
  yoffset *= sensitivity;

  yaw += xoffset;
  pitch += yoffset;

  if (pitch > 89.0f) pitch = 89.0f;
  if (pitch < -89.0f) pitch = -89.0f;

  glm::vec3 front;
  front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
  front.y = sin(glm::radians(pitch));
  front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
  cameraFront = glm::normalize(front);
}

/**
 * @brief Main execution
 */
int main(void) {
  // Initialize GLFW
  if (!glfwInit()) {
    fprintf(stderr, "Failed to initialize GLFW\n");
    return -1;
  }

  glfwWindowHint(GLFW_SAMPLES, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  // Make the window
  window = glfwCreateWindow(800, 600, "BMP Texture Rectangle", NULL, NULL);
  if (!window) {
    fprintf(stderr, "Failed to open GLFW window\n");
    glfwTerminate();
    return -1;
  }
  glfwMakeContextCurrent(window);

  // GLEW initializer
  glewExperimental = true;
  if (glewInit() != GLEW_OK) {
    fprintf(stderr, "Failed to initialize GLEW\n");
    return -1;
  }

  // Frame buffer things
  int fbWidth, fbHeight;
  glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
  glViewport(0, 0, fbWidth, fbHeight);

  // Set background color
  glClearColor(0.2f, 0.3f, 0.3f, 1.0f);

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);

  // Do some work for defining transparent sphere parameters
  std::vector<glm::vec3> sphereVertices;
  std::vector<GLuint> sphereIndices;
  int stacks = 20;
  int slices = 20;
  float radius = 10.0f;  // 10m radius
  for (int i = 0; i <= stacks; ++i) {
    float phi = glm::pi<float>() * i / stacks;
    for (int j = 0; j <= slices; ++j) {
      float theta = 2.0f * glm::pi<float>() * j / slices;
      float x = radius * sin(phi) * cos(theta);
      float y = radius * cos(phi);
      float z = radius * sin(phi) * sin(theta);
      sphereVertices.push_back(glm::vec3(x, y, z));
    }
  }

  // generate indices for triangles
  for (int i = 0; i < stacks; ++i) {
    for (int j = 0; j < slices; ++j) {
      int first = i * (slices + 1) + j;
      int second = first + slices + 1;

      sphereIndices.push_back(first);
      sphereIndices.push_back(second);
      sphereIndices.push_back(first + 1);

      sphereIndices.push_back(second);
      sphereIndices.push_back(second + 1);
      sphereIndices.push_back(first + 1);
    }
  }

  // Sphere vertex things
  GLuint sphereVAO, sphereVBO, sphereEBO;
  glGenVertexArrays(1, &sphereVAO);
  glGenBuffers(1, &sphereVBO);
  glGenBuffers(1, &sphereEBO);

  glBindVertexArray(sphereVAO);

  glBindBuffer(GL_ARRAY_BUFFER, sphereVBO);
  glBufferData(GL_ARRAY_BUFFER, sphereVertices.size() * sizeof(glm::vec3),
               sphereVertices.data(), GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphereEBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sphereIndices.size() * sizeof(GLuint),
               sphereIndices.data(), GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
  glEnableVertexAttribArray(0);

  glBindVertexArray(0);

  // Need to do some work to construct the field. Start with length and width
  float fieldWidth = 10.0f;   // X-axis width
  float fieldLength = 50.0f;  // Z-axis length
  GLfloat fieldVertices[] = {
      -fieldWidth / 2, 0.0f, -fieldLength / 2, 0.0f, 0.0f,
      fieldWidth / 2,  0.0f, -fieldLength / 2, 1.0f, 0.0f,
      fieldWidth / 2,  0.0f, fieldLength / 2,  1.0f, 1.0f,
      -fieldWidth / 2, 0.0f, fieldLength / 2,  0.0f, 1.0f,
  };

  GLuint fieldIndices[] = {0, 1, 2, 2, 3, 0};

  // Construct field vertex arrays, buffers, etc
  GLuint fieldVAO, fieldVBO, fieldEBO;
  glGenVertexArrays(1, &fieldVAO);
  glGenBuffers(1, &fieldVBO);
  glGenBuffers(1, &fieldEBO);

  glBindVertexArray(fieldVAO);

  glBindBuffer(GL_ARRAY_BUFFER, fieldVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(fieldVertices), fieldVertices,
               GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, fieldEBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(fieldIndices), fieldIndices,
               GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);

  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                        (void*)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  glBindVertexArray(0);

  // Load our shaders
  GLuint programID = LoadShaders("StandardShading.vertexshader",
                                 "StandardShading.fragmentshader");
  if (programID == 0) {
    fprintf(stderr, "Shader program failed to load (programID == 0)\n");
    return -1;
  }

  // Setup mouse callback for mouse camera angle movement
  glfwSetCursorPosCallback(window, mouseCallback);
  glfwSetInputMode(window, GLFW_CURSOR,
                   GLFW_CURSOR_DISABLED);  // hide & capture cursor

  GLuint texture;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);

  // Load football field bmp using stbi
  int width, height, nrChannels;
  unsigned char* data = stbi_load("ff.bmp", &width, &height, &nrChannels, 0);
  if (data) {
    printf("Loaded texture: %d x %d, channels=%d\n", width, height, nrChannels);
  } else {
    fprintf(stderr, "Failed to load texture (data == NULL)\n");
  }

  // Load uav obj (chicken)
  std::vector<float> verts, uvs, norms;
  if (!loadOBJ("chicken_01.obj", verts, uvs, norms)) {
    printf("OBJ load failed!\n");
  }

  glm::vec3 minV(FLT_MAX), maxV(-FLT_MAX);
  for (auto& v : verts) {
    minV = glm::min(minV, v);
    maxV = glm::max(maxV, v);
  }

  glm::vec3 dims = maxV - minV;
  float maxDim =
      std::max(dims.x, std::max(dims.y, dims.z));  // use this later for scaling

  GLuint objVAO, objVBO;
  glGenVertexArrays(1, &objVAO);
  glGenBuffers(1, &objVBO);

  glBindVertexArray(objVAO);

  glBindBuffer(GL_ARRAY_BUFFER, objVBO);
  glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(),
               GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);

  if (data) {
    GLenum format = (nrChannels == 3) ? GL_RGB : GL_RGBA;
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format,
                 GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    // Texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  } else {
    fprintf(stderr, "Failed to load texture\n");
  }
  stbi_image_free(data);

  glUseProgram(programID);
  glUniform1i(glGetUniformLocation(programID, "myTextureSampler"), 0);

  // Camera settings
  glm::vec3 cameraPos = glm::vec3(0.0f, 0.5f, 5.0f);

  float deltaTime = 0.0f;
  float lastFrame = 0.0f;

  float cameraSpeed = 7.0f;  // units per second

  GLuint MatrixID = glGetUniformLocation(programID, "MVP");

  std::vector<float> yardLines = {0.0f, 10.0f, 20.0f, 30.0f, 40.0f};

  // UAV positions container
  std::vector<glm::vec3> UAVPositions;

  for (float yard : yardLines) {
    auto zPos = yard;
    UAVPositions.push_back(
        glm::vec3((-fieldWidth / 2.0f) + 1.0f, 0.0f, zPos));  // left
    UAVPositions.push_back(glm::vec3(0.0, 0.0f, zPos));
    UAVPositions.push_back(
        glm::vec3((fieldWidth / 2.0f) - 1.0f, 0.0f, zPos));  // right
  }

  // Generate the collection of uavs, and call start() on each
  std::vector<std::unique_ptr<ECE_UAV>> uavs;
  for (int i = 0; i < (int)UAVPositions.size(); ++i) {
    auto newUav = std::make_unique<ECE_UAV>(UAVPositions[i]);
    newUav->start();
    uavs.push_back(std::move(newUav));
  }

  bool startCountdown = false;
  double countdownStartTime = 0.0;

  // Main render loop
  while (!glfwWindowShouldClose(window)) {
    // Some handling for polling. Check the last poll time and current time, and
    // do an update if the difference is over 30 ms
    static double lastPoll = glfwGetTime();
    double now = glfwGetTime();
    if (now - lastPoll >= 0.03) {
      // Update lastPoll and get uav positions
      lastPoll = now;
      for (size_t i = 0; i < uavs.size(); ++i) {
        glm::vec3 p = uavs[i]->getPosition();
      }

      // do collision checks
      // For each uav, see if any of the other uavs is close enough to hit it
      for (size_t i = 0; i < uavs.size(); ++i) {
        for (size_t j = i + 1; j < uavs.size(); ++j) {
          glm::vec3 posA = uavs[i]->getPosition();
          glm::vec3 posB = uavs[j]->getPosition();

          float dist = glm::length(posA - posB);
          float minDist = uavs[i]->getSize() * 0.5f + uavs[j]->getSize() * 0.5f;

          if (dist < minDist) {
            // collision detected: swap velocities
            // lock both UAVs to prevent race conditions
            std::unique_lock<std::mutex> lock1(uavs[i]->getMutex(),
                                               std::defer_lock);
            std::unique_lock<std::mutex> lock2(uavs[j]->getMutex(),
                                               std::defer_lock);
            std::lock(lock1, lock2);  // needed to lock both safely

            uavs[i]->swapVelocity(*uavs[j]);
          }
        }
      }
    }

    // Set camera front
    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);

    float currentFrame = glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    float velocity = cameraSpeed * deltaTime;

    // Key handling to allow for moving around the map
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

    glm::mat4 View = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
    glm::mat4 Projection =
        glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);

    // Lets draw the football field
    glm::mat4 fieldModel = glm::mat4(1.0f);
    fieldModel = glm::translate(
        fieldModel, glm::vec3(0.0f, -0.01f, 20.0f));  // slightly below UAVs
    glm::mat4 fieldMVP = Projection * View * fieldModel;
    glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &fieldMVP[0][0]);

    // Bind solid color to football field (see shader code for handling of
    // useSolidColor)
    glUniform1i(glGetUniformLocation(programID, "useSolidColor"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    glBindVertexArray(fieldVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    // Check if all uavs have entered sphere mode
    bool allInSphereMode = true;
    for (int i = 0; i < uavs.size(); i++) {
      if (!startCountdown && !uavs[i]->getIsInSphereMode()) {
        // If any of them aren't in sphere mode, we don't enter the 60 second
        // waiting period yet
        allInSphereMode = false;
      }
      glm::vec3 position = uavs[i]->getPosition();

      glm::mat4 Model = glm::mat4(1.0f);
      Model = glm::translate(Model, position);
      auto scalingFactor = uavs[i]->getSize() / maxDim;

      Model = glm::scale(Model, glm::vec3(scalingFactor));
      Model = glm::rotate(Model, glm::radians(180.0f), glm::vec3(0, 1, 0));

      glm::mat4 MVP = Projection * View * Model;
      glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);

      glBindVertexArray(objVAO);

      // Some handling for uav color oscillation. We want intensity to be at 0.5
      // hertz, so get time and apply 2pi
      // * 0.5
      float t = glfwGetTime();
      float intensity = 0.75f + 0.25f * sin(2.0f * 3.14159f * 0.5f * t);
      glm::vec3 baseColor(1.0, 0.5, 0.0);  // orange (arbitrary)

      // oscillates between 0.5 and 1.0
      glUniform1f(glGetUniformLocation(programID, "uColorIntensity"),
                  intensity);
      glUniform1i(glGetUniformLocation(programID, "useSolidColor"), 1);
      glUniform1i(glGetUniformLocation(programID, "useOscillation"), 1);
      glUniform3fv(glGetUniformLocation(programID, "solidColor"), 1,
                   &baseColor[0]);

      // Draw
      glDrawArrays(GL_TRIANGLES, 0, verts.size() / 3);
    }

    // If we are all in sphere mode, start the timer
    if (!startCountdown && allInSphereMode) {
      startCountdown = true;
      countdownStartTime = glfwGetTime();  // mark start of 60-second timer
    }

    if (startCountdown) {
      double now = glfwGetTime();
      if (now - countdownStartTime >= 60.0) {
        // 60 seconds have passed, terminate sim
        glfwSetWindowShouldClose(window, true);
      }

      // print time remaining to console
      double remaining = 60.0 - (glfwGetTime() - countdownStartTime);
      std::cout << "Time remaining: " << remaining << " seconds\r"
                << std::flush;
    }

    // draw the sphere
    glm::mat4 sphereModel =
        glm::translate(glm::mat4(1.0f),
                       glm::vec3(0.0f, 50.0f, 0.0f));  // same as mSphereCenter
    glm::mat4 sphereMVP = Projection * View * sphereModel;
    glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &sphereMVP[0][0]);

    // set some shader conditions to make sure we get solid coloring on the
    // sphere
    glUniform1i(glGetUniformLocation(programID, "useSolidColor"), 1);
    glUniform1i(glGetUniformLocation(programID, "useOscillation"), 0);
    glUniform3f(glGetUniformLocation(programID, "solidColor"), 0.0f, 1.0f,
                1.0f);  // blue, arbitrary

    // wireframe
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    // Draw sphere
    glBindVertexArray(sphereVAO);
    glDrawElements(GL_TRIANGLES, sphereIndices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    // need to reset this
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // Swap buffers and poll events
    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  // Once sim is over, stop() all uavs
  for (auto& u : uavs) {
    u->stop();
  }

  // join threads
  for (auto& u : uavs) {
    u->join();
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
