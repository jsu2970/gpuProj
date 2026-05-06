#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/shader.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>


#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <cfloat>

#define USE_FLASH_SHADER

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// camera
Camera camera(glm::vec3(-7.0f, -3.0f, 20.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// 손전등 on/off를 위한 변수
bool flashlightOn = true;
bool fKeyPressed = false;

int main()
{
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    //glfwSetScrollCallback(window, scroll_callback);  // 확대 기능은 비활성화

    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // tell stb_image.h to flip loaded texture's on the y-axis (before loading model).
    stbi_set_flip_vertically_on_load(true);

    // configure global opengl state
    // -----------------------------
    glEnable(GL_DEPTH_TEST);

    // build and compile shaders
    // -------------------------
#ifndef USE_FLASH_SHADER
    Shader ourShader("shader/1.model_loading.vs", "shader/1.model_loading.fs");
#else
    Shader lightingShader("shader/5.4.light_casters.vs", "shader/5.4.light_casters.fs");
#endif

    // load models
    // -----------
    Model ourModel("resources/counter-strike_italy_3d/scene.gltf");

    Model flashlightModel("resources/flash_light/scene.gltf");
        
    // draw in wireframe
    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
      
    // render loop
    // -----------
    while (!glfwWindowShouldClose(window))
    {
        // per-frame time logic
        // --------------------
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // input
        // -----
        processInput(window);

        // render
        // ------
        glClearColor(0.25f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

#ifndef USE_FLASH_SHADER
        // don't forget to enable shader before setting uniforms
        ourShader.use();

        // view/projection transformations
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);

        // render the loaded model
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f)); // translate it down so it's at the center of the scene
        model = glm::scale(model, glm::vec3(1.0f, 1.0f, 1.0f));	// it's a bit too big for our scene, so scale it down
        ourShader.setMat4("model", model);
        ourModel.Draw(ourShader);
   

#else
        // be sure to activate shader when setting uniforms/drawing objects
        lightingShader.use();
        lightingShader.setVec3("light.position", camera.Position);
        lightingShader.setVec3("light.direction", camera.Front);
        lightingShader.setFloat("light.cutOff", glm::cos(glm::radians(7.0f)));
        lightingShader.setFloat("light.outerCutOff", glm::cos(glm::radians(12.0f)));
        lightingShader.setVec3("viewPos", camera.Position);

        // light properties
        if (flashlightOn)
        {
            lightingShader.setVec3("light.ambient", 0.02f, 0.02f, 0.02f);
            lightingShader.setVec3("light.diffuse", 0.35f, 0.35f, 0.35f);
            lightingShader.setVec3("light.specular", 0.15f, 0.15f, 0.15f);
        }
        else
        {
            lightingShader.setVec3("light.ambient", 0.0f, 0.0f, 0.0f);
            lightingShader.setVec3("light.diffuse", 0.0f, 0.0f, 0.0f);
            lightingShader.setVec3("light.specular", 0.0f, 0.0f, 0.0f);
        }

        lightingShader.setFloat("light.constant", 1.0f);
        lightingShader.setFloat("light.linear", 0.25f);
        lightingShader.setFloat("light.quadratic", 0.20f);

        // material properties
        lightingShader.setFloat("shininess", 32.0f);

        // view/projection transformations
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        lightingShader.setMat4("projection", projection);
        lightingShader.setMat4("view", view);

        // 카스 맵 그리기
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f)); // translate it down so it's at the center of the scene
        model = glm::scale(model, glm::vec3(0.01f, 0.01f, 0.01f));	// it's a bit too big for our scene, so scale it down

        // 전등의 위치를 가져옴
        vector<glm::vec3> lampPositions = ourModel.GetLightPositionsFromMaterialParts("material_32", model);
        int count = std::min((int)lampPositions.size(), 64);

        lightingShader.setInt("lampLightCount", count);  // 사용하는 전등 개수를 넣음

        // 쉐이더의 전등 위치값과 빛 처리 상수값 초기화
        for (int i = 0; i < count; i++)
        {
            std::string idx = std::to_string(i);

            lightingShader.setVec3("lampLights[" + idx + "].position", lampPositions[i]);
            lightingShader.setVec3("lampLights[" + idx + "].ambient", 0.002f, 0.006f, 0.005f);
            lightingShader.setVec3("lampLights[" + idx + "].diffuse", 0.08f, 0.22f, 0.18f);
            lightingShader.setVec3("lampLights[" + idx + "].specular", 0.02f, 0.06f, 0.05f);

            lightingShader.setFloat("lampLights[" + idx + "].constant", 1.0f);
            lightingShader.setFloat("lampLights[" + idx + "].linear", 0.18f);
            lightingShader.setFloat("lampLights[" + idx + "].quadratic", 0.12f);
        }  

        ourModel.Draw(lightingShader, model);

        // 손전등 모델 그리기 (카메라에 부착)
        glm::mat4 flashModel = glm::mat4(1.0f);

        // 손전등 위치 계산
        // 카메라 위치에서 앞쪽(Front)으로 0.5, 오른쪽(Right)으로 0.2, 아래(Up)로 0.2만큼 떨어진 지점
        glm::vec3 flashlightPos = camera.Position + (camera.Front * 0.5f) + (camera.Right * 0.25f) - (camera.Up * 0.25f);

        flashModel = glm::translate(flashModel, flashlightPos); // 위치만 이동!

        // 카메라의 로컬 좌표축을 모델 행렬에 직접 주입 (회전 동기화 핵심)
        // 카메라가 보는 방향을 모델의 축으로 삼습니다.
        flashModel[0] = glm::vec4(camera.Right, 0.0f);
        flashModel[1] = glm::vec4(camera.Up, 0.0f);
        flashModel[2] = glm::vec4(-camera.Front, 0.0f); // OpenGL은 오른손 좌표계이므로 -Front

        // (C) 모델 초기 회전 보정 (여기가 중요!)
        flashModel = glm::rotate(flashModel, glm::radians(-80.0f), glm::vec3(0, 1, 0));
        flashModel = glm::rotate(flashModel, glm::radians(75.0f), glm::vec3(0.1, 0, 1));

        flashModel = glm::scale(flashModel, glm::vec3(0.03f)); // 모델 크기에 맞게 조절 [cite: 796]
        
        flashlightModel.Draw(lightingShader, flashModel);
#endif

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    glfwTerminate();
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);

    // f 버튼으로 손전등을 껐다 킴
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS && !fKeyPressed)
    {
        flashlightOn = !flashlightOn;
        fKeyPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_RELEASE)
    {
        fKeyPressed = false;
    }
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}
