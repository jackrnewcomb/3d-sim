/*
Author: Jack Newcomb
Class: ECE6122
Last date modified: 10/19/2025

Description:

This is a modified version of the controls.cpp file. This mostly follows the tutorial, but adds some logic for the
requested keyboard controls for the assignment. In particular, we add handling for WASD inputs, and up/down for radial
up/down camera movement. Due to the up/down handling, theta and phi (representing horizontal and vertical angles,
respectively) were implemented. These are translated back to cartesian space for the final position vector.

*/

// Include GLFW
#include <GLFW/glfw3.h>
extern GLFWwindow *window; // The "extern" keyword here is to access the variable "window" declared in tutorialXXX.cpp.
                           // This is a hack to keep the tutorials simple. Please avoid this.

// Include GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
using namespace glm;

#include "controls.hpp"

glm::mat4 ViewMatrix;
glm::mat4 ProjectionMatrix;

glm::mat4 getViewMatrix()
{
    return ViewMatrix;
}
glm::mat4 getProjectionMatrix()
{
    return ProjectionMatrix;
}

// Initial position : on +Z
glm::vec3 position = glm::vec3(0, 0, 10);
// Initial horizontal angle : toward -Z
float horizontalAngle = 3.14f;
// Initial vertical angle : none
float verticalAngle = 0.0f;
// Initial Field of View
float initialFoV = 60.0f;

float speed = 3.0f; // 3 units / second

void computeMatricesFromInputs()
{
    static double lastTime = glfwGetTime();

    // Define horizontal angle
    static float theta = 0.0f;

    // Define vertical angle
    static float phi = 0.0f;

    // Get time delta
    double currentTime = glfwGetTime();
    float deltaTime = float(currentTime - lastTime);

    // Set origin to 0,0,0
    glm::vec3 origin = glm::vec3(0, 0, 0);

    // Developer preference... camera rotation speed and zoom speed respectively
    float orbitSpeed = 1.5f;
    float zoomSpeed = 10.0f;

    // Handle input
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    {
        // Rotate radially left by altering horizontal angle
        theta -= orbitSpeed * deltaTime;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    {
        // Rotate radially right by altering horizontal angle
        theta += orbitSpeed * deltaTime;
    }
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
    {
        // Rotate radially up by altering vertical angle
        phi += orbitSpeed * deltaTime;
    }
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
    {
        // Rotate radially down by altering vertical angle
        phi -= orbitSpeed * deltaTime;
    }

    // This allows full orbit in the vertical angle
    float pi = 3.14159265f;
    if (phi > pi)
    {
        phi -= 2 * pi;
    }
    if (phi < -pi)
    {
        phi += 2 * pi;
    }

    // Determine radius, aka zoom
    static float r = glm::length(position);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    {
        // Decrease the radius by zooming in
        r -= zoomSpeed * deltaTime;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    {
        // Increase the radius by zooming out
        r += zoomSpeed * deltaTime;
    }

    // Get back into cartesian space using phi and theta to find x,y,z
    position.x = r * cos(phi) * sin(theta);
    position.y = r * sin(phi);
    position.z = r * cos(phi) * cos(theta);

    // Ensure we always are looking at the origin (all rotation requirements are radial, so camera should always be
    // looking at monkeys)
    glm::vec3 direction = glm::normalize(origin - position);
    glm::vec3 up = glm::vec3(0, 1, 0);
    glm::vec3 right = glm::normalize(glm::cross(up, direction));
    up = glm::cross(direction, right);

    // Projection & View
    float FoV = 60.0f;
    ProjectionMatrix = glm::perspective(glm::radians(FoV), 4.0f / 3.0f, 0.1f, 100.0f);
    ViewMatrix = glm::lookAt(position, origin, up);

    lastTime = currentTime;
}
