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

#include <algorithm>
#include <cmath>

#define USE_FLASH_SHADER

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);
float GetGroundY(glm::vec3 pos);  // 오르막 계산을 위한 위치 검사
bool CheckWallCollision(glm::vec3 pos);  // 충돌 검사
bool RayIntersectsTriangle(glm::vec3 rayOrigin, glm::vec3 rayDir, const Triangle& tri, float& t);
float DistancePointToSegment2D(glm::vec2 p, glm::vec2 a, glm::vec2 b);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// camera
Camera camera(glm::vec3(-7.0f, -3.0f, 20.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// 충돌 삼각형. 이 변수를 통해 플레이어가 부딪힐 곳을 설정함
vector<Triangle> collisionTriangles;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// 손전등 on/off를 위한 변수
bool flashlightOn = true;
bool fKeyPressed = false;

// 유저 충돌을 위한 변수
float eyeHeight = 0.7f;  // 눈 높이 (카메라 높이)
float playerHeight = 0.85f;  // 캐릭터 전체 키
float playerRadius = 0.2f;  // 몸통 반지름

float stepHeight = 0.2f;   // 올라갈 수 있는 최대 높이
float fallHeight = 1.0f;   // 아래로 찾을 최대 깊이

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
    Model ourModel("resources/counter-strike_italy_3d/scene.gltf");  // 맵 로드

    glm::mat4 mapMatrix = glm::mat4(1.0f);
    mapMatrix = glm::scale(mapMatrix,glm::vec3(0.01f));  // 맵의 크기가 0.01이므로 스케일링 함
    collisionTriangles = ourModel.GetCollisionTriangles(mapMatrix);  // 충돌 삼각형들을 모두 계산함
    cout << "collision tris: " << collisionTriangles.size() << endl;

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
        lightingShader.setFloat("light.cutOff", glm::cos(glm::radians(8.0f)));
        lightingShader.setFloat("light.outerCutOff", glm::cos(glm::radians(15.0f)));
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
        lightingShader.setFloat("light.linear", 0.09f);
        lightingShader.setFloat("light.quadratic", 0.032f);

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
        
        glDisable(GL_DEPTH_TEST);
        flashlightModel.Draw(lightingShader, flashModel);
        glEnable(GL_DEPTH_TEST);
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
    // 종료 버튼
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // 이동 전 위치 저장
    glm::vec3 oldPos = camera.Position;

    glm::vec3 desiredPos = camera.Position;

    // 이동 버튼
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);

    // 일단 원래 위치로 되돌림
    desiredPos = camera.Position;

    // 핵심: 실제 위치를 다시 oldPos로 되돌린 뒤 x/z를 따로 시도
    camera.Position = oldPos;

    // x 이동만 시도
    glm::vec3 tryX = camera.Position;
    tryX.x = desiredPos.x;

    float groundX = GetGroundY(tryX);
    if (groundX > -9999.0f)
        tryX.y = groundX + eyeHeight;

    if (!CheckWallCollision(tryX))
        camera.Position = tryX;

    // z 이동만 시도
    glm::vec3 tryZ = camera.Position;
    tryZ.z = desiredPos.z;

    float groundZ = GetGroundY(tryZ);
    if (groundZ > -9999.0f)
        tryZ.y = groundZ + eyeHeight;

    if (!CheckWallCollision(tryZ))
        camera.Position = tryZ;

    //// 현재 카메라 위치 아래로 ray를 쏴서 지금 발 밑에 있는 바닥 삼각형 높이를 구함
    //float ground = GetGroundY(camera.Position);

    //if (ground > -9999.0f)  // 바닥의 초기값을 -10000.0f로 두어서 유효한 바닥을 찾았는지 검사함
    //{
    //    camera.Position.y = ground + eyeHeight;  // ground는 발바닥 높이지만 카메라는 사람 눈 위치기 때문에 키 만큼 y축 높이를 더함
    //}
    //else  // 바닥을 찾지 못한 경우 (맵 밖, 구멍 등) 기존 높이를 유지함
    //{
    //    camera.Position.y = oldPos.y;
    //}

    //// 벽에 부딪혔는지 검사하여 부딪히면 이전 위치로 이동함
    //if (CheckWallCollision(camera.Position))
    //{
    //    camera.Position = oldPos;
    //}

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

// 현재 유저 위치 아래에 있는 바닥 삼각형의 y 높이를 찾는 함수
float GetGroundY(glm::vec3 pos)
{
    float feetY = pos.y - eyeHeight;  // 발 위치

    glm::vec3 rayOrigin = glm::vec3(pos.x, feetY + stepHeight, pos.z);  // 발 근처에서 ray를 쏨 
    glm::vec3 rayDir = glm::vec3(0.0f, -1.0f, 0.0f);  // ray 방향을 위에서 아래로 잡음 (위 -> 아래 ray)

    float closestY = -10000.0f;  // 초기 상태 (바닥을 찾지 못하면이 값이 변하지 않음)
    float closestT = FLT_MAX;  // 현재 ray가 맞은 가장 가까운 triangle 거리를 저장하는 변수

    for (const Triangle& tri : collisionTriangles)  // 맵 전체 triangle을 순회함
    {
        // 위를 향하는 면만 바닥으로 취급. normal.y값이 작으면 벽이나 천장임
        if (tri.normal.y < 0.15f)
            continue;

        float t;  // 바닥의 위치를 저장하는 변수
        if (RayIntersectsTriangle(rayOrigin, rayDir, tri, t))  // ray와 삼각형 충돌 여부를 검사함
        {
            //if (t < closestT)  // 지하가 존재할 수 있으므로 가장 가까운 바닥을 찾음
            //{
            //    closestT = t;  
            //    glm::vec3 hitPoint = rayOrigin + rayDir * t;  // ray 위의 실제 충돌 지점을 계산함
            //    closestY = hitPoint.y;  // ray와 triangle이 실제로 만난 위치 중 y좌표만 뽑아 씀
            //}
            glm::vec3 hitPoint = rayOrigin + rayDir * t;

            // 너무 높은 천장은 바닥으로 보지 않음
            if (hitPoint.y > feetY + stepHeight)
                continue;

            // 너무 아래층은 무시
            if (hitPoint.y < feetY - fallHeight)
                continue;

            if (t < closestT)
            {
                closestT = t;
                closestY = hitPoint.y;
            }
        }
    }

    return closestY;
}

// ray가 triangle과 충돌하는지 검사하는 Möller–Trumbore ray triangle intersection 알고리즘
bool RayIntersectsTriangle(glm::vec3 rayOrigin, glm::vec3 rayDir, const Triangle& tri, float& t)
{
    const float EPSILON = 0.0000001f;

    glm::vec3 edge1 = tri.b - tri.a;
    glm::vec3 edge2 = tri.c - tri.a;

    glm::vec3 h = glm::cross(rayDir, edge2);
    float det = glm::dot(edge1, h);

    if (det > -EPSILON && det < EPSILON)
        return false;

    float invDet = 1.0f / det;

    glm::vec3 s = rayOrigin - tri.a;
    float u = invDet * glm::dot(s, h);

    if (u < 0.0f || u > 1.0f)
        return false;

    glm::vec3 q = glm::cross(s, edge1);
    float v = invDet * glm::dot(rayDir, q);

    if (v < 0.0f || u + v > 1.0f)
        return false;

    t = invDet * glm::dot(edge2, q);

    return t > EPSILON;
}


// 삼각형 기반으로 유저가 벽에 닿았는지 검사함
bool CheckWallCollision(glm::vec3 pos)
{
    for (const Triangle& tri : collisionTriangles)
    {
        // 바닥이나 천장은 벽 충돌에서 제외
        if (fabs(tri.normal.y) > 0.15f)
            continue;

        float triMinY = std::min(tri.a.y, std::min(tri.b.y, tri.c.y));  // 벽 삼각형의 가장 낮은 y값
        float triMaxY = std::max(tri.a.y, std::max(tri.b.y, tri.c.y));  // 벽 삼각형의 가장 높은 y값

        float playerFeetY = pos.y - eyeHeight;  // 플레이어의 발 높이
        float playerHeadY = playerFeetY + playerHeight;  // 플레이어의 머리 높이

        // 발 근처의 낮은 턱/계단 앞면은 벽으로 막지 않음
        if (triMaxY <= playerFeetY + stepHeight)
            continue;

        // 플레이어의 y축 위치와 벽 삼각형의 y축 위치 범위가 서로 일치하지 않으면 충돌 검사를 하지 않음 (현관 위의 벽은 부딪히지 않음)
        if (playerHeadY < triMinY || playerFeetY > triMaxY) 
            continue;

        // 벽 충돌은 위에서 내려다본 기준으로 검사하면 되기 때문에 x z 평면으로 변경함
        glm::vec2 p(pos.x, pos.z);
        glm::vec2 a(tri.a.x, tri.a.z);
        glm::vec2 b(tri.b.x, tri.b.z);
        glm::vec2 c(tri.c.x, tri.c.z);

        // 플레이어의 중심과 삼각형 변 사이의 거리를 계산함
        float d1 = DistancePointToSegment2D(p, a, b);
        float d2 = DistancePointToSegment2D(p, b, c);
        float d3 = DistancePointToSegment2D(p, c, a);

        // 반지름 안이면 충돌 판정
        if (d1 < playerRadius || d2 < playerRadius || d3 < playerRadius)
            return true;
    }

    // 충돌하지 않으면 false
    return false;
}

// 점과 선분 사이의 최소 거리를 계산하는 함수
float DistancePointToSegment2D(glm::vec2 p, glm::vec2 a, glm::vec2 b)
{
    glm::vec2 ab = b - a;
    float len2 = glm::dot(ab, ab);

    if (len2 < 0.00001f)
        return glm::length(p - a);

    float t = glm::dot(p - a, ab) / len2;
    t = glm::clamp(t, 0.0f, 1.0f);

    glm::vec2 closest = a + t * ab; 
    return glm::length(p - closest);
}