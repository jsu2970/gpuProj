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

// 사운드 라이브러리
#include <AL/al.h>
#include <AL/alc.h>  

#define DR_WAV_IMPLEMENTATION
#include <AL/dr_wav.h>

/*
    특정 좌표를 지나면 post-processing 효과, 유령 이벤트 발생을 위해 해당 좌표를 영역으로 저장하는 구조체임

    minX ---------------- maxX
      |                      |
      |       문 영역        |
      |                      |
    minZ ---------------- maxZ
*/
struct TriggerZone {
    float minX;
    float maxX;
    float minY;
    float maxY;
    float minZ;
    float maxZ;
};

struct Sound;
struct GhostEvent;
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);
float GetGroundY(glm::vec3 pos);  // 오르막 계산을 위한 위치 검사
bool CheckWallCollision(glm::vec3 pos);  // 충돌 검사
bool RayIntersectsTriangle(glm::vec3 rayOrigin, glm::vec3 rayDir, const Triangle& tri, float& t);
float DistancePointToSegment2D(glm::vec2 p, glm::vec2 a, glm::vec2 b);
bool IsInsideZone(glm::vec3 pos, TriggerZone zone);
bool CrossedLine(float prev, float curr, float line);
bool NearDoorCrossed(glm::vec3 prevPos, glm::vec3 currPos, TriggerZone door, char axis, float line);
bool InitOpenAL();
void DeleteSound(Sound& sound);
void ShutdownOpenAL();
ALuint LoadWav(const char* filename);
Sound CreateSound(const char* filePath, float volume, bool loop, glm::vec3 position, bool is3D);
glm::vec3 GetRandomSoundPositionAroundPlayer(glm::vec3 playerPos);
void UpdateGhostEvent(GhostEvent& ghost, glm::vec3 prevPlayerPos, glm::vec3 playerPos, char axis, float line, float deltaTime);
void RenderGhostEvent(GhostEvent& ghost, Shader& shader, Model& ghostModel);
glm::vec3 FixGhostPos(glm::vec3 pos);

// settings
const unsigned int SCR_WIDTH = 1600;
const unsigned int SCR_HEIGHT = 900;

// camera
Camera camera(glm::vec3(-7.0f, -3.0f, 20.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// 충돌 삼각형. 이 변수를 통해 플레이어가 부딪힐 곳을 설정함
vector<Triangle> collisionTriangles;

// 중력 변수
float verticalVelocity = 0.0f;
float gravity = -9.8f;
bool isGrounded = false;

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

struct GhostEvent
{
    TriggerZone trigger;

    glm::vec3 startPos;
    glm::vec3 endPos;

    bool active = false;
    bool finished = false;

    float moveT = 0.0f;
    float speed = 0.45f;
};

// 선을 밟고 지나가는 유령 이벤트
GhostEvent ghostFarOutside;
GhostEvent ghostWineStorage;
GhostEvent ghostHostageRoad;

// 인질 집 안 고정 유령
GhostEvent hostageHouseGhost;

// OpenAL에서 사용할 사운드 하나를 buffer + source로 묶는 구조체
struct Sound
{
    ALuint buffer = 0;  // wav 데이터가 저장된 OpenAL buffer ID
    ALuint source = 0;  // 실제로 소리를 재생하는 OpenAL source ID 
};

// 사운드 설정을 위한 전역변수
ALCdevice* audioDevice = nullptr;  // 스피커/오디오 장치
ALCcontext* audioContext = nullptr;  // OpenAL 작업 공간

ALuint footstepBuffer;  // 실제 소리 데이터(mp3/wav 내용)
ALuint footstepSource;  // 소리를 재생하는 위치/오브젝트

ALuint buzzBuffer;
ALuint buzzSource;

// 사운드 효과 활성화 여부
bool soundEnabled;

// 전역 사운드 객체
vector<Sound> footstepSounds;
Sound bgmSound;
Sound heartBeatSound;
vector<Sound> electronicSounds;  // 전등 여러 개의 지지직 소리
Sound metalClangSound;
Sound flashLightSound;
Sound landSound;  // 착지 소리

// 착지 판정용
bool wasGrounded = true;
float fallStartY = 0.0f;
bool jumpStarted = false;      // 스페이스바 점프 착지음용
bool realFallStarted = false;  // 높은 곳 낙하 착지음용

// metal clang 랜덤 재생
float clangTimer = 0.0f;              // metal clang 타이머
float nextClangTime = 20.0f;          // 시작 후 첫 소리는 20초 뒤
bool firstClangPlayed = false;        // 첫 clang 여부

// cube shadow map 설정
const unsigned int SHADOW_WIDTH = 1024;
const unsigned int SHADOW_HEIGHT = 1024;

// 2개의 전등까지 쉐도우 매핑을 사용함
const int MAX_SHADOW_LAMPS = 2;

unsigned int depthMapFBO[MAX_SHADOW_LAMPS];
unsigned int depthCubemap[MAX_SHADOW_LAMPS];
int shadowLampIndices[MAX_SHADOW_LAMPS] = { -1, -1 };
glm::vec3 shadowLightPositions[MAX_SHADOW_LAMPS];

float near_plane = 0.1f;
float far_plane = 20.0f;  // 전등 그림자가 보일 최대 거리

int main()
{
    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

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

    Shader lightingShader("shader/5.4.light_casters.vs", "shader/5.4.light_casters.fs");

    // point light cube shadow map용 depth shader
    Shader depthShader(
        "shader/point_shadow_depth.vs",
        "shader/point_shadow_depth.fs"
    );

    // 일렁이는 물체를 그리기 위한 쉐이더
    Shader wobbleShader("shader/wobble.vs", "shader/wobble.fs");

    // ===============================
    // Point Light Shadow Cubemap 생성
    // ===============================

    // depthMapFBO는 shadow map 전용 framebuffer로 그림자용 임시 랜더링 공간을 만듦
    glGenFramebuffers(MAX_SHADOW_LAMPS, depthMapFBO);
    glGenTextures(MAX_SHADOW_LAMPS, depthCubemap);

    for (int s = 0; s < MAX_SHADOW_LAMPS; s++)
    {
        glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap[s]);

        for (unsigned int i = 0; i < 6; ++i)
        {
            glTexImage2D(
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,  // 6개의 면을 대상으로 함
                0,
                GL_DEPTH_COMPONENT,  // 색 RGB 저장하지 않고 depth값만 저장함 (픽셀이 빛으로부터 얼마나 가까운지 판단함)
                SHADOW_WIDTH,  // 쉐도우 맵의 해상도로 너무 낮으면 그림자에 계단현상이 발생함
                SHADOW_HEIGHT,
                0,
                GL_DEPTH_COMPONENT,
                GL_FLOAT,
                NULL
            );
        }

        // shadow map은 색이 아니라 depth만 필요함
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        // cubemap 경계에서 이상한 선이 생기지 않도록 clamp 설정
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        // framebuffer에 depth cubemap 연결
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO[s]);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthCubemap[s], 0);

        // 색상 버퍼는 사용하지 않음
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            std::cout << "ERROR::SHADOW:: Depth cubemap framebuffer is not complete!" << std::endl;
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // load models
    // -----------
    Model ourModel("resources/counter-strike_italy_3d/scene.gltf");  // 맵 로드

    glm::mat4 mapMatrix = glm::mat4(1.0f);
    mapMatrix = glm::scale(mapMatrix,glm::vec3(0.01f));  // 맵의 크기가 0.01이므로 스케일링 함
    collisionTriangles = ourModel.GetCollisionTriangles(mapMatrix);  // 충돌 삼각형들을 모두 계산함
    cout << "collision tris: " << collisionTriangles.size() << endl;

    Model something("resources/there_is_something/scene.gltf", 2);  // 움직이는 물체 로드 (2번 메테리얼만 로드)

    Model flashlightModel("resources/flash_light/scene.gltf");  // 손전등 로드

    // post-processing위해 유저가 지나갔을 때 트리거가 발동할 공간의 margin값
    float marginXZ = 0.5f;
    float marginY = 0.15f;

    // 머나먼 곳 와인창고 입구
    TriggerZone door1A = {
        -10.2488f - marginXZ, -10.2488f + marginXZ,
        -0.42f - marginY, -0.42f + marginY,
        -12.0614f - marginXZ, -12.0614f + marginXZ
    };

    // 다리 아래 와인창고 입구
    TriggerZone door1B = {
        -2.48595f - marginXZ, -2.48595f + marginXZ,
        -2.02f - marginY, -2.02f + marginY,
        -16.0774f - marginXZ, -16.0774f + marginXZ
    };

    // 테러리스트 인질 건물 입구
    TriggerZone door2A = {
        9.35265f - marginXZ, 9.35265f + marginXZ,
        -0.5f - marginY, -0.5f + marginY,
        -19.94f - marginXZ, -19.94f + marginXZ
    };

    // 테러리스트 인질 건물 배란다
    TriggerZone door2B = {
        9.22476f - marginXZ, 9.22476f + marginXZ,
        0.78f - marginY, 0.78f + marginY,
        -19.8493f - marginXZ, -19.8493f + marginXZ
    };

    // 옥상 아래 건물 입구
    TriggerZone door3A = {
        -8.19693f - marginXZ, -8.19693f + marginXZ,
        -0.42f - marginY, -0.42f + marginY,
        -6.5113f - marginXZ, -6.5113f + marginXZ
    };

    // 대테러리스트 방향 배란다
    TriggerZone door3B = {
        -4.31245f - marginXZ, -4.31245f + marginXZ,
        -0.82f - marginY, -0.82f + marginY,
        2.82221f - marginXZ, 2.82221f + marginXZ
    };

    // 유저가 건물 안에 있는지 판별하는 값
    bool insideBuilding = false;

    // 이전 프레임에 건물 안이었는지 저장
    bool wasInsideBuilding = false;

    // 유저의 이전 위치를 저장하는 변수
    glm::vec3 prevPlayerPos = camera.Position;

    // ===============================
    // 유령 이벤트 좌표 설정
    // ===============================

    // 1. 머나먼곳 바깥
    // 첫 번째 좌표: 밟는 선
    // 두 번째 좌표: 유령 시작 위치
    // 세 번째 좌표: 유령 이동 목표 위치
    ghostFarOutside.trigger = {  // 트리거되는 영역을 만듦
        -11.1636f - marginXZ, -11.1636f + marginXZ,
        -2.1f - marginY,  -2.1f + marginY,
         1.22582f - marginXZ, 1.22582f + marginXZ
    };
    // 유령이 이동하는 좌표
    ghostFarOutside.startPos = FixGhostPos(glm::vec3(-11.6769f, -0.5f, -11.1092f));
    ghostFarOutside.endPos = FixGhostPos(glm::vec3(-10.3508f, -0.42f, -11.127f));
    ghostFarOutside.speed = 1.0f;

    // 2. 와인창고 안
    ghostWineStorage.trigger = {
        -10.3399f - marginXZ, -10.3399f + marginXZ,
        -2.02f - marginY,  -2.02f + marginY,
        -18.5095f - marginXZ, -18.5095f + marginXZ
    };
    ghostWineStorage.startPos = FixGhostPos(glm::vec3(-3.30236f, -2.02f, -23.6281f));
    ghostWineStorage.endPos = FixGhostPos(glm::vec3(-3.2292f, -2.02f, -17.8712f));
    ghostWineStorage.speed = 0.7f;

    // 3. 인질집 가는 길
    ghostHostageRoad.trigger = {
         7.93203f - marginXZ,  7.93203f + marginXZ,
        -1.46f - marginY,     -1.46f + marginY,
        -8.72986f - marginXZ, -8.72986f + marginXZ
    };
    ghostHostageRoad.startPos = FixGhostPos(glm::vec3(7.0576f, 0.78f, -20.4068f));
    ghostHostageRoad.endPos = FixGhostPos(glm::vec3(6.0635f, 0.78f, -20.4007f));
    ghostHostageRoad.speed = 1.0f;

    // 4. 인질 집 안
    // 이건 선 넘기 판정 없이 영역 밟으면 바로 고정 유령 생성
    hostageHouseGhost.trigger = {
         5.67541f - marginXZ,  5.67541f + marginXZ,
         0.78f - marginY,   0.78f + marginY,
        -23.3189f - marginXZ, -23.3189f + marginXZ
    };
    hostageHouseGhost.startPos = FixGhostPos(glm::vec3(-2.27924f, -0.5f, -17.424f));
    hostageHouseGhost.endPos = hostageHouseGhost.startPos; // 안 움직임

    // post-processing을 위한 기본 처리들
    float horrorAmount = 0.0f;  // 호러 필터를 천천히 씌우기 위한 변수 (0.0f: 필터 없음, 1.0f: 필터 100% 적용)

    float quadVertices[] = {
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

    unsigned int quadVAO, quadVBO;

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);

    glBindVertexArray(quadVAO);

    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    // 위치
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    // 텍스처 좌표
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);

    // 스크린 쉐이더 생성
    Shader screenShader("shader/screen.vs", "shader/screen.fs");

    screenShader.use();
    screenShader.setInt("screenTexture", 0);

    // framebuffer configuration
    // -------------------------
    unsigned int framebuffer;
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    // create a color attachment texture
    unsigned int textureColorbuffer;
    glGenTextures(1, &textureColorbuffer);
    glBindTexture(GL_TEXTURE_2D, textureColorbuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureColorbuffer, 0);
    // create a renderbuffer object for depth and stencil attachment (we won't be sampling these)
    unsigned int rbo;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, SCR_WIDTH, SCR_HEIGHT); // use a single renderbuffer object for both a depth AND stencil buffer.
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo); // now actually attach it
    // now that we actually created the framebuffer and added all attachments we want to check if it is actually complete now
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 사운드 초기화 하기
    soundEnabled = InitOpenAL();

    if (!soundEnabled)  // 리턴값이 false면 사운드 비활성화
    {
        std::cout << "Sound disabled\n";
    }
    else  // 사운드 파일들을 실제로 로딩
    {
        // 발자국 사운드 여러 개 로딩
        footstepSounds.push_back(CreateSound("resources/sounds/footstep1.wav", 0.40f, false, glm::vec3(0.0f), false));
        footstepSounds.push_back(CreateSound("resources/sounds/footstep2.wav", 0.60f, false, glm::vec3(0.0f), false));
        footstepSounds.push_back(CreateSound("resources/sounds/footstep3.wav", 0.40f, false, glm::vec3(0.0f), false));

        heartBeatSound = CreateSound("resources/sounds/heart_beat.wav", 0.40f, false, glm::vec3(0.0f), false);

        // metal clang에만 따로 소리 감쇠 적용
        metalClangSound = CreateSound("resources/sounds/metal_clang.wav", 0.3f, false, glm::vec3(0.0f), true);
        alSourcef(metalClangSound.source, AL_REFERENCE_DISTANCE, 5.0f);
        alSourcef(metalClangSound.source, AL_MAX_DISTANCE, 40.0f);
        alSourcef(metalClangSound.source, AL_ROLLOFF_FACTOR, 1.0f);

        flashLightSound = CreateSound("resources/sounds/flash_light.wav", 0.40f, false, glm::vec3(0.0f), false);
        landSound = CreateSound("resources/sounds/jump_land.wav", 0.45f, false, glm::vec3(0.0f), false);

        bgmSound = CreateSound("resources/sounds/bgm.wav", 0.08f, true, glm::vec3(0.0f), false);  // 반복 재생 사운드는 초기화 직후 재생
        alSourcePlay(bgmSound.source);

        // 전등 위치
        vector<glm::vec3> electricPositions = {
            glm::vec3(-13.2192f, -2.02f, -11.533f),
            glm::vec3(-11.4779f, -2.02f, 3.48009f),
            glm::vec3(-2.84282f, -2.02f, 2.64737f),
            glm::vec3(1.56382f, -2.02f, 0.556459f),  // 쉐도우 매핑시 사라질 수 있음
            glm::vec3(7.03281f, -1.94f, -4.84839f),
            glm::vec3(10.2782f, -2.42f, -6.12697f)
        };

        for (glm::vec3 pos : electricPositions) {
            Sound s = CreateSound("resources/sounds/electric.wav", 0.3f, true, pos, true);  // 위치별로 전등 소리 생성

            if (s.source != 0) {
                alSourcePlay(s.source);  // 소리를 재생
                electronicSounds.push_back(s);
            }
        }
    }
        
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

        // OpenAL listener를 현재 카메라 위치/방향으로 매 프레임 갱신
        if (soundEnabled)
        {
            // listener 위치 = 플레이어 귀 위치 = 카메라 위치
            alListener3f(AL_POSITION, camera.Position.x, camera.Position.y, camera.Position.z);

            // listener 방향 = 카메라가 보는 방향 + 카메라의 위쪽 방향
            float listenerOrientation[] =
            {
                camera.Front.x, camera.Front.y, camera.Front.z,
                camera.Up.x,    camera.Up.y,    camera.Up.z
            };

            alListenerfv(AL_ORIENTATION, listenerOrientation);
        }

        // 유저가 서 있는 위치를 1초마다 출력
        static float lastPrint = 0.0f;
        if (glfwGetTime() - lastPrint > 1.0f)
        {
            lastPrint = glfwGetTime();

            cout << "Player Pos: "
                << camera.Position.x << ", "
                << camera.Position.y << ", "
                << camera.Position.z << endl;
        }

        // render
        // ------

        // 카스 맵 그리기
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f)); // translate it down so it's at the center of the scene
        model = glm::scale(model, glm::vec3(0.01f, 0.01f, 0.01f));	// it's a bit too big for our scene, so scale it down

        // 전등의 위치를 가져옴
        vector<glm::vec3> lampPositions = ourModel.GetLightPositionsFromMaterialParts("material_32", model);
        int count = std::min((int)lampPositions.size(), 64);

        // 플레이어와 가장 가까운 전등을 찾음. shadow map은 전등 2개만 생성 (최적화)
        shadowLampIndices[0] = -1;
        shadowLampIndices[1] = -1;

        float minDist1 = FLT_MAX;  // 현재까지 발견한, 전등과 가장 가까운 거리
        float minDist2 = FLT_MAX;

        for (int i = 0; i < count; i++)
        {
            // 플레이어와 전등 거리 계산
            float dist = glm::length(camera.Position - lampPositions[i]);

            // 더 가까운 전등 발견 시 갱신
            if (dist < minDist1)
            {
                minDist2 = minDist1;
                shadowLampIndices[1] = shadowLampIndices[0];

                minDist1 = dist;
                shadowLampIndices[0] = i;
            }
            else if (dist < minDist2)
            {
                minDist2 = dist;
                shadowLampIndices[1] = i;
            }
        }

        // shadow map 생성에 사용할 전등 위치
        for (int s = 0; s < MAX_SHADOW_LAMPS; s++)
        {
            if (shadowLampIndices[s] != -1)  // 가까운 전등을 발견한 경우
                shadowLightPositions[s] = lampPositions[shadowLampIndices[s]];  // 찾은 전등 위치를 넣음
            else
                shadowLightPositions[s] = glm::vec3(0.0f);  // 발견하지 못한 경우
        }

        // 가장 가까운 전등 2개를 기준으로 depth cubemap을 렌더링함
        for (int s = 0; s < MAX_SHADOW_LAMPS; s++)
        {
            if (shadowLampIndices[s] == -1)
                continue;

            glm::vec3 shadowLightPos = shadowLightPositions[s];

            float aspect = (float)SHADOW_WIDTH / (float)SHADOW_HEIGHT;  // 화면 비율 값으로 cube face는 정사각형임

            glm::mat4 shadowProj = glm::perspective(
                glm::radians(90.0f),  // 한 면이 담당해야 할 범위는 90도임 (앞, 뒤, 오른쪽, 왼쪽 = 360도)
                aspect,
                near_plane,  // 이 값보다 가까운 물체는 그림자를 그리지 않음
                far_plane  // 이 값보다 먼 물체는 그림자를 그리지 않음
            );

            // point light는 6방향을 봐야 하므로 viewProjection 행렬 6개 생성
            std::vector<glm::mat4> shadowTransforms;

            shadowTransforms.push_back(shadowProj * glm::lookAt(
                shadowLightPos,   // 전등 위치
                shadowLightPos + glm::vec3(1, 0, 0),  // 빛나는 물체 기준 +x축
                glm::vec3(0, -1, 0)));  // 해당 cube에 맞는 up벡터
            shadowTransforms.push_back(shadowProj * glm::lookAt(shadowLightPos, shadowLightPos + glm::vec3(-1, 0, 0), glm::vec3(0, -1, 0)));
            shadowTransforms.push_back(shadowProj * glm::lookAt(shadowLightPos, shadowLightPos + glm::vec3(0, 1, 0), glm::vec3(0, 0, 1)));
            shadowTransforms.push_back(shadowProj * glm::lookAt(shadowLightPos, shadowLightPos + glm::vec3(0, -1, 0), glm::vec3(0, 0, -1)));
            shadowTransforms.push_back(shadowProj * glm::lookAt(shadowLightPos, shadowLightPos + glm::vec3(0, 0, 1), glm::vec3(0, -1, 0)));
            shadowTransforms.push_back(shadowProj * glm::lookAt(shadowLightPos, shadowLightPos + glm::vec3(0, 0, -1), glm::vec3(0, -1, 0)));

            glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);  // 쉐도우 매핑 기준으로 변경
            glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO[s]);

            glClearDepth(1.0f);
            glDepthFunc(GL_LESS);
            glDepthMask(GL_TRUE);
            glEnable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);

            depthShader.use();
            depthShader.setFloat("far_plane", far_plane);
            depthShader.setVec3("lightPos", shadowLightPos);

            // cubemap 6면을 반복해서 랜더함
            for (unsigned int face = 0; face < 6; face++)
            {
                glFramebufferTexture2D(
                    GL_FRAMEBUFFER,
                    GL_DEPTH_ATTACHMENT,
                    GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                    depthCubemap[s],
                    0
                );

                glClear(GL_DEPTH_BUFFER_BIT);

                depthShader.setMat4("shadowMatrix", shadowTransforms[face]);

                // 맵 depth 기록
                ourModel.Draw(depthShader, model);
            }
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);

        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glEnable(GL_DEPTH_TEST);

        // 안개 색상 정의 및 화면 지우기
        glm::vec3 fogColor = glm::vec3(0.02f, 0.04f, 0.04f);
        glClearColor(fogColor.r, fogColor.g, fogColor.b, 1.0f); // 배경색을 안개색과 일치
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 셰이더 활성화
        lightingShader.use();

        // shadow cubemap을 texture unit에 연결
        lightingShader.setInt("depthMaps[0]", 30);
        lightingShader.setInt("depthMaps[1]", 31);

        lightingShader.setFloat("far_plane", far_plane);

        lightingShader.setInt("shadowLampIndices[0]", shadowLampIndices[0]);
        lightingShader.setInt("shadowLampIndices[1]", shadowLampIndices[1]);

        lightingShader.setVec3("shadowLightPositions[0]", shadowLightPositions[0]);
        lightingShader.setVec3("shadowLightPositions[1]", shadowLightPositions[1]);

        glActiveTexture(GL_TEXTURE30);
        glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap[0]);

        glActiveTexture(GL_TEXTURE31);
        glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap[1]);

        // 모든 유니폼 변수 설정
        lightingShader.setVec3("fogColor", fogColor);
        lightingShader.setFloat("fogDensity", 0.15f);
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

        // 손전등 모델 초기 회전 보정 
        flashModel = glm::rotate(flashModel, glm::radians(-80.0f), glm::vec3(0, 1, 0));
        flashModel = glm::rotate(flashModel, glm::radians(75.0f), glm::vec3(0.1, 0, 1));

        flashModel = glm::scale(flashModel, glm::vec3(0.03f)); // 모델 크기에 맞게 조절 [cite: 796]

        glDisable(GL_DEPTH_TEST);
        flashlightModel.Draw(lightingShader, flashModel);
        glEnable(GL_DEPTH_TEST);

        // 현재 위치를 저장하여 이전 위치와 비교해 post-processing, 유령 이벤트 효과를 냄
        glm::vec3 playerPos = camera.Position;

        // 유령 쉐이더
        wobbleShader.use();
        wobbleShader.setMat4("projection", projection);
        wobbleShader.setMat4("view", view);
        wobbleShader.setFloat("time", glfwGetTime());

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glDisable(GL_CULL_FACE);

        glm::mat4 partialModelMatrix = glm::mat4(1.0f);

        partialModelMatrix = glm::translate(
            partialModelMatrix,
            glm::vec3(-7.0f, -2.9f, 21.0f)
        );

        //partialModelMatrix = glm::rotate(model, glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 0.0f));

        partialModelMatrix = glm::scale(
            partialModelMatrix,
            glm::vec3(0.47f)
        );

        //something.Draw(wobbleShader, partialModelMatrix);

        // ===============================
        // 유령 이벤트 업데이트
        // ===============================

        // 머나먼곳 바깥
        // x축 기준으로 -11.1636f 선을 넘었는지 검사
        UpdateGhostEvent(
            ghostFarOutside,
            prevPlayerPos,
            playerPos,
            'x',
            -11.1636f,
            deltaTime
        );

        // 와인창고 안
        // z축 기준으로 -18.5095f 선을 넘었는지 검사
        UpdateGhostEvent(
            ghostWineStorage,
            prevPlayerPos,
            playerPos,
            'z',
            -18.5095f,
            deltaTime
        );

        // 인질집 가는 길
        // z축 기준으로 -8.72986f 선을 넘었는지 검사
        UpdateGhostEvent(
            ghostHostageRoad,
            prevPlayerPos,
            playerPos,
            'z',
            -8.72986f,
            deltaTime
        );

        // 인질 집 안 고정 유령
        // 선 넘기 판정 없이 영역 안에 들어오면 바로 활성화
        if (!hostageHouseGhost.finished) {
            bool inside = IsInsideZone(playerPos, hostageHouseGhost.trigger);

            if (inside) {
                hostageHouseGhost.active = true;
            }
            else if (hostageHouseGhost.active) {
                // 한 번 보였는데 영역 밖으로 나가면 종료
                hostageHouseGhost.active = false;
                hostageHouseGhost.finished = true;
            }
        }
        
        RenderGhostEvent(ghostFarOutside, wobbleShader, something);
        RenderGhostEvent(ghostWineStorage, wobbleShader, something);
        RenderGhostEvent(ghostHostageRoad, wobbleShader, something);
        RenderGhostEvent(hostageHouseGhost, wobbleShader, something);  // 고정 유령
        
        // 머나먼 곳 종료 조건
        if (ghostFarOutside.active && ghostFarOutside.moveT >= 1.0f)
        {
            ghostFarOutside.active = false;
            ghostFarOutside.finished = true;
        }

        // 와인 창고 종료 조건
        if (ghostWineStorage.active && ghostWineStorage.moveT >= 1.0f)
        {
            ghostWineStorage.active = false;
            ghostWineStorage.finished = true;
        }

        // 인질 집 가는 길 종료 조건
        if (ghostHostageRoad.active && ghostHostageRoad.moveT >= 1.0f)
        {
            ghostHostageRoad.active = false;
            ghostHostageRoad.finished = true;
        }

        // 유령 이벤트 종료시 기존으로 돌아감
        glEnable(GL_CULL_FACE);
        glDisable(GL_BLEND);

        // 카메라에 post-processing 효과를 넣음. 유저가 특정 영역을 지나는지 검사

        // 1번 건물 A문: z가 작아지면 안쪽
        if (NearDoorCrossed(prevPlayerPos, playerPos, door1A, 'z', -12.0614f))
        {
            insideBuilding = playerPos.z < -12.0614f;
        }
        // 1번 건물 B문: x가 작아지면 안쪽
        if (NearDoorCrossed(prevPlayerPos, playerPos, door1B, 'x', -2.48595f))
        {
            insideBuilding = playerPos.x < -2.48595f;
        }
        // 2번 건물 A문: z가 작아지면 안쪽
        if (NearDoorCrossed(prevPlayerPos, playerPos, door2A, 'z', -19.94f))
        {
            insideBuilding = playerPos.z < -19.94f;
        }
        // 2번 건물 B문: z가 작아지면 안쪽
        if (NearDoorCrossed(prevPlayerPos, playerPos, door2B, 'z', -19.8493f))
        {
            insideBuilding = playerPos.z < -19.8493f;
        }
        // 3번 건물 A문: z가 커지면 안쪽
        if (NearDoorCrossed(prevPlayerPos, playerPos, door3A, 'z', -6.5113f))
        {
            insideBuilding = playerPos.z > -6.5113f;
        }
        // 3번 건물 B문: x가 작아지면 안쪽
        if (NearDoorCrossed(prevPlayerPos, playerPos, door3B, 'x', -4.31245f))
        {
            insideBuilding = playerPos.x < -4.31245f;
        }

        float targetAmount = insideBuilding ? 1.0f : 0.0f;  // 목표 필터 값으로 건물 안이면 필터 100%인 1.0f를, 건물 밖이면 필터 0%인 0.0f 값을 갖게 됨
        float fadeSpeed = 0.7f; // 클수록 필터가 빨리 적용됨

        if (horrorAmount < targetAmount)  // 현재 필터 상태가 목표 필터보다 작으면 조금 증가시킴
        {
            horrorAmount += fadeSpeed * deltaTime;  // 프레임 당 서서히 필터가 적용됨
            if (horrorAmount > 1.0f)  // horrorAmount값이 1.0f를 넘지 못하도록 제한
                horrorAmount = 1.0f;
        }
        else if (horrorAmount > targetAmount)  // 건물 밖으로 나갔을 때의 케이스
        {
            horrorAmount -= fadeSpeed * deltaTime;
            if (horrorAmount < 0.0f)
                horrorAmount = 0.0f;
        }

        if (soundEnabled && heartBeatSound.source != 0)
        {
            if (!wasInsideBuilding && insideBuilding)  // 건물 내부로 들어갈때만 심장박동 재생 (1회)
            {
                alSourcePlay(heartBeatSound.source);
            }
        }
        wasInsideBuilding = insideBuilding;  // 현재 상태를 다음 프레임 비교용으로 저장

        prevPlayerPos = playerPos;  // 유저의 이전 위치 저장

        // 후처리 셰이더로 화면 사각형 렌더링
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDisable(GL_DEPTH_TEST);
        glClear(GL_COLOR_BUFFER_BIT);

        screenShader.use();
        screenShader.setFloat("horrorAmount", horrorAmount);  // 필터 적용 여부
        screenShader.setInt("horrorMode", 1);  // 필터 모드
        screenShader.setFloat("time", glfwGetTime());

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureColorbuffer);

        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);  // 화면 전체에 그리기

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // glfw: terminate, clearing all previously allocated GLFW resources.
    // ------------------------------------------------------------------
    if (soundEnabled)  // 프로그램 종료 전 OpenAL 리소스 정리
    {
        ShutdownOpenAL();
    }

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

    // 이동 후 목표 위치 (벽에 충돌하지 않는다면 그대로 적용됨)
    desiredPos = camera.Position;

    // 핵심: 실제 위치를 다시 oldPos로 되돌린 뒤 x/z를 따로 시도
    camera.Position = oldPos;

    // x 이동만 시도
    glm::vec3 tryX = camera.Position;  // 이동 전 위치를 tryX에 넣음
    tryX.x = desiredPos.x;  // tryX값에 이동 후 x좌표를 넣음

    if (!CheckWallCollision(tryX))  // tryX로 x축만 이동해서 충돌이 일어나지 않은 경우
        camera.Position = tryX;  // 카메라 위치를 x축은 이동한 것으로 판정 (만약 충돌난다면 x축은 기존 oldPos값을 쓰게 됨)

    // z 이동만 시도
    glm::vec3 tryZ = camera.Position;
    tryZ.z = desiredPos.z;

    if (!CheckWallCollision(tryZ))
        camera.Position = tryZ;  // 여기까지의 결과가 합쳐져서 x, z 둘 다 움직였다면 처음 목표 이동 위치인 desiredPos와 같아짐

    // 스페이스바를 눌렀을 때 점프 처리
    // 조건: 스페이스바가 눌려 있어야 하고, 캐릭터가 바닥에 닿아 있는 상태(isGrounded)여야 함
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && isGrounded)
    {
        // 점프 힘 설정 (숫자가 클수록 더 높이 점프함)
        float jumpHeight = 2.5f;

        // 위 방향으로 속도를 즉시 부여
        verticalVelocity = jumpHeight;
        
        // 점프 소리
        jumpStarted = true;
        realFallStarted = false;

        // 점프를 시작하면 더 이상 바닥 상태가 아님
        isGrounded = false;
    }

    // 중력 계산
    // 현재 카메라 위치 아래로 ray를 쏴서 지금 발 밑에 있는 바닥 삼각형 높이를 구함
    float ground = GetGroundY(camera.Position);  

    if (ground > -9999.0f)  // 바닥의 초기값을 -10000.0f로 두어서 유효한 바닥을 찾았는지 검사함
    {
        verticalVelocity += gravity * deltaTime;  // 속도가 점점 아래 방향으로 증가

        float nextCameraY = camera.Position.y + verticalVelocity * deltaTime;  // 다음 카메라 위치를 미리 구함
        float nextFeetY = nextCameraY - eyeHeight;  // 다음 발 위치를 미리 구함

        /*
        * currentFeetY는 유저가 바닥에 서 있을 때 있어야 하는 발 위치임
        * ground = 0, eyeHeight = 0.7 이라면 유저가 정상적으로 서 있다면 카메라 y값은 0.7임
        * 그런데 카메라 위치가 8로 높은 위치에 존재한다면 떨어지고 있는 중이므로 해당 값들을 비교하면서 중력을 계산함
        */ 
        if (nextFeetY <= ground)  // 다음 발 위치가 바닥보다 아래로 내려가려 한다면 착지 처리함
        {
            camera.Position.y = ground + eyeHeight;  // 내려왔다면 유저가 서 있어야 할 눈 높이를 카메라 y축에 넣음

            float fallDistance = fallStartY - camera.Position.y;

            // 착지 사운드 재생
            if (!wasGrounded && soundEnabled && landSound.source != 0 && (jumpStarted || (realFallStarted && fallDistance > 0.25f)))  // 점프했거나, 떨어지는 거리가 긴 경우 소리를 냄
            {
                float volume = glm::clamp(fallDistance / 3.0f, 0.3f, 1.0f);

                alSourcef(landSound.source, AL_GAIN, volume);

                alSourceStop(landSound.source);
                alSourcePlay(landSound.source);
            }
            // 땅에 있는 상태로 변경
            jumpStarted = false;
            realFallStarted = false;

            verticalVelocity = 0.0f;  // 떨어지는 속도를 0으로 함
            isGrounded = true;
        }
        else
        {
            camera.Position.y = nextCameraY;  // 현재 카메라 위치에 y축 값을 넣음
            isGrounded = false;

            // 점프가 아닌 상태로 충분히 아래로 떨어지면 실제 낙하로 인정
            float currentFallDistance = fallStartY - camera.Position.y;
            if (!jumpStarted && currentFallDistance > 0.25f)
            {
                realFallStarted = true;
            }
        }
    }
    else  // 유효한 바닥을 찾지 못한 경우 (맵 밖으로 떨어진 경우)
    {
        verticalVelocity += gravity * deltaTime;  // 속도가 점점 아래 방향으로 증가
        camera.Position.y += verticalVelocity * deltaTime;  // 실제로 아래로 이동
        isGrounded = false;
    }

    // 이번 프레임 중력 처리 결과로 처음 공중이 된 순간
    if (!isGrounded && wasGrounded)
    {
        fallStartY = oldPos.y;
    }

    // 이동 중이면 일정 간격마다 발자국 소리 재생
    if (soundEnabled)
    {
        static float footstepTimer = 0.0f;
        float footstepInterval = 0.52f;  // 발소가 간격
        float movedDistance = glm::length(camera.Position - oldPos);  // 이동 거리 계산

        bool moved = movedDistance > 0.001f;  // 이동 거리가 거의 없는 경우 발자국 소리를 내지 않음

        if (moved && (isGrounded || realFallStarted == false))  // 공중에 떠있으면 소리가 나지 않음
        {
            footstepTimer += deltaTime;

            if (footstepTimer >= footstepInterval)  // 발소리 간격만큼 시간이 지났는가?
            {
                // 랜덤 발자국 재생
                if (!footstepSounds.empty())
                {
                    int randomStep = rand() % footstepSounds.size();

                    alSourceStop(footstepSounds[randomStep].source);
                    alSourcePlay(footstepSounds[randomStep].source);
                }

                footstepTimer = 0.0f;  // 발소리를 냈다면 발소리 간격 타이머를 처음부터 다시 셈
            }
        }
        else  // 멈췄다가 다시 움직이면 즉시 발소리부터 시작
        {
            footstepTimer = 0.0f;
        }
    }

    // 처음으로 움직인 이후 일정 시간 뒤 metal clang 한 번 재생
    if (soundEnabled && !firstClangPlayed)
    {
        clangTimer += deltaTime;

        // 일정 시간이 지나면 실행
        if (clangTimer >= nextClangTime)
        {
            // 유저 주변 랜덤 위치 계산
            glm::vec3 clangPos = GetRandomSoundPositionAroundPlayer(camera.Position);

            // metal clang 위치 변경
            alSource3f(metalClangSound.source, AL_POSITION, clangPos.x, clangPos.y, clangPos.z);

            // 소리 재생
            alSourcePlay(metalClangSound.source);

            // 다시 실행되지 않게 설정
            firstClangPlayed = true;
        }
    }

    // f 버튼으로 손전등을 껐다 킴
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS && !fKeyPressed)
    {
        flashlightOn = !flashlightOn;
        fKeyPressed = true;

        // 손전등 클릭 소리 재생
        if (soundEnabled && flashLightSound.source != 0)
        {
            alSourceStop(flashLightSound.source); // 연타 시 처음부터 재생
            alSourcePlay(flashLightSound.source);
        }
    }
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_RELEASE)
    {
        fKeyPressed = false;
    }

    // 다음 프레임 비교용
    wasGrounded = isGrounded;
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
            glm::vec3 hitPoint = rayOrigin + rayDir * t;  // ray 위의 실제 충돌 지점을 계산함

            // 너무 높은 천장은 바닥으로 보지 않음
            if (hitPoint.y > feetY + stepHeight)
                continue;

            // 너무 아래층은 무시
            if (hitPoint.y < feetY - fallHeight)
                continue;

            if (t < closestT)  // 지하가 존재할 수 있으므로 가장 가까운 바닥을 찾음
            {
                closestT = t;
                closestY = hitPoint.y;  // ray와 triangle이 실제로 만난 위치 중 y좌표만 뽑아 씀
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

// 현재 유저가 문 영역 안에 있는지 검사하는 함수임
bool IsInsideZone(glm::vec3 pos, TriggerZone zone)
{
    return  pos.x >= zone.minX && pos.x <= zone.maxX &&
        pos.y >= zone.minY && pos.y <= zone.maxY &&
        pos.z >= zone.minZ && pos.z <= zone.maxZ;
}

// 유저가 문 영역을 넘어서 정말로 건물 안으로 들어왔는지 검사하는 함수임
bool CrossedLine(float prev, float curr, float line)  // prev는 이전 위치, curr는 현재 위치
{
    return (prev < line && curr >= line) ||
        (prev >= line && curr < line);
}

// 이전 또는 현재 위치 둘 중 하나라도 유저가 문 영역 안에 있었는지 검사하는 함수
bool NearDoorCrossed(glm::vec3 prevPos, glm::vec3 currPos, TriggerZone door, char axis, float line)
{
    // 이전 위치 혹은 현재 위치가 영역 안인가?
    bool nearDoor = IsInsideZone(prevPos, door) || IsInsideZone(currPos, door);

    if (!nearDoor)
        return false;  // 아니라면 검사 안함

    // 문의 line이 x축 기준인 경우
    if (axis == 'x')
        return CrossedLine(prevPos.x, currPos.x, line);

    // 문의 line이 z축 기준인 경우
    if (axis == 'z')
        return CrossedLine(prevPos.z, currPos.z, line);

    return false;  // 건물 안쪽으로 간게 아니면 false
}

// OpenAL (사운드)를 초기화 하는 함수
bool InitOpenAL()
{
    audioDevice = alcOpenDevice(nullptr);  // 컴퓨터의 실제 오디오 장치에 연결 (윈도우 기본 스피커 사용)
    if (!audioDevice)
    {
        std::cout << "OpenAL device failed\n";
        return false;
    }

    audioContext = alcCreateContext(audioDevice, nullptr);  // OpenAL 작업공간 생성
    if (!audioContext)
    {
        std::cout << "OpenAL context failed\n";
        return false;
    }

    alcMakeContextCurrent(audioContext);  // 앞으로 OpenAL 명령은 audiContext에 적용한다는 의미
    return true;  // OpenAL 준비 완료
}

// OpenAL을 종료하는 함수
void DeleteSound(Sound& sound)
{
    if (sound.source != 0)
    {
        alDeleteSources(1, &sound.source);
        sound.source = 0;
    }

    if (sound.buffer != 0)
    {
        alDeleteBuffers(1, &sound.buffer);
        sound.buffer = 0;
    }
}

void ShutdownOpenAL()
{
    DeleteSound(bgmSound);
    DeleteSound(heartBeatSound);
    DeleteSound(metalClangSound);
    DeleteSound(flashLightSound);
    DeleteSound(landSound);
    // 여러 발소리 사운드 삭제
    for (Sound& s : footstepSounds)
    {
        DeleteSound(s);
    }
    footstepSounds.clear();
    // 여러 전등 사운드 삭제
    for (Sound& s : electronicSounds)
    {
        DeleteSound(s);
    }
    electronicSounds.clear();

    alcMakeContextCurrent(nullptr);

    if (audioContext)
    {
        alcDestroyContext(audioContext);
        audioContext = nullptr;
    }

    if (audioDevice)
    {
        alcCloseDevice(audioDevice);
        audioDevice = nullptr;
    }
}

// .wav 파일을 읽어서 OpenAL buffer로 변환하는 함수 (OpenAL은 오직 재생 역할만 담당함)
ALuint LoadWav(const char* filename)
{
    unsigned int channels;
    unsigned int sampleRate;
    drwav_uint64 totalPCMFrameCount;

    int16_t* sampleData = drwav_open_file_and_read_pcm_frames_s16(
        filename,
        &channels,
        &sampleRate,
        &totalPCMFrameCount,
        nullptr
    );

    if (!sampleData)
    {
        std::cout << "Failed to load wav\n";
        return 0;
    }

    ALenum format =
        (channels == 1)
        ? AL_FORMAT_MONO16
        : AL_FORMAT_STEREO16;

    ALuint buffer;

    alGenBuffers(1, &buffer);

    alBufferData(
        buffer,
        format,
        sampleData,
        totalPCMFrameCount * channels * sizeof(int16_t),
        sampleRate
    );

    drwav_free(sampleData, nullptr);

    std::cout << filename
        << " channels: " << channels
        << ", sampleRate: " << sampleRate
        << std::endl;

    return buffer;
}

// 사운드 객체를 생성함
Sound CreateSound(
    const char* filePath,  // 재생할 wav 파일 경로
    float volume,  // 소리 크기
    bool loop,  // 반복 재생 여부
    glm::vec3 position,  // 소리 위치
    bool is3D  // true는 위치 기반 소리, false는 일반 효과음 재생
)
{
    Sound sound;

    sound.buffer = LoadWav(filePath);

    if (sound.buffer == 0)
    {
        std::cout << "Sound load failed: " << filePath << std::endl;
        return sound;
    }

    alGenSources(1, &sound.source);

    alSourcei(sound.source, AL_BUFFER, sound.buffer);
    alSourcef(sound.source, AL_GAIN, volume);
    alSourcei(sound.source, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);

    // 3D 사운드면 월드 좌표를 적용
    if (is3D)
    {
        alSource3f(
            sound.source,
            AL_POSITION,
            position.x,
            position.y,
            position.z
        );

        // 3d 사운드 거리 감쇠
        alSourcef(sound.source, AL_REFERENCE_DISTANCE, 3.0f);  // 가까이 왔을 때만 크게 들림
        alSourcef(sound.source, AL_MAX_DISTANCE, 30.0f);  // 이 거리가 되면 거의 안들림
        alSourcef(sound.source, AL_ROLLOFF_FACTOR, 15.0f);  // 거리 감쇠를 강하게
        alSourcef(sound.source, AL_MIN_GAIN, 0.0f);   // 멀어지면 완전 무음 허용
    }

    return sound;
}

// metal clang을 재생하기 위한 랜덤 위치를 계산하는 함수
glm::vec3 GetRandomSoundPositionAroundPlayer(glm::vec3 playerPos)
{
    // 유저로부터 얼마나 떨어질지 랜덤 거리
    float distance = 4.0f + static_cast<float>(rand()) / RAND_MAX * 4.0f;

    // 수정: 0~360도 랜덤 방향
    float angle = static_cast<float>(rand()) / RAND_MAX * glm::two_pi<float>();

    // 원형으로 랜덤 위치 계산
    float x = playerPos.x + cos(angle) * distance;
    float z = playerPos.z + sin(angle) * distance;

    return glm::vec3(x, playerPos.y, z);  // 위치 반환
}

// 유령 이벤트 업데이트 함수
void UpdateGhostEvent(GhostEvent& ghost, glm::vec3 prevPlayerPos, glm::vec3 playerPos, char axis, float line, float deltaTime) {
    // 이미 한 번 끝난 이벤트면 다시 실행하지 않음
    if (ghost.finished)
        return;

    // 아직 유령이 나오지 않은 상태라면 기존 post-processing 문 판정처럼 트리거 영역 근처에서 선을 넘었는지 검사함
    if (!ghost.active) {
        if (NearDoorCrossed(prevPlayerPos, playerPos, ghost.trigger, axis, line)) {
            ghost.active = true;
            ghost.moveT = 0.0f;
        }
    }

    // 유령이 활성화되면 startPos에서 endPos까지 이동
    if (ghost.active) {
        ghost.moveT += deltaTime * ghost.speed;

        // moveT가 1.0이면 목표 지점 도착
        if (ghost.moveT >= 1.0f) {
            ghost.moveT = 1.0f;
        }
    }
}

// 유령 렌더링 함수
void RenderGhostEvent(GhostEvent& ghost, Shader& shader, Model& ghostModel) {
    // 활성화 된 경우에만 그림
    if (!ghost.active)
        return;

    // moveT 값에 따라 시작 위치에서 목표 위치까지 부드럽게 이동
    glm::vec3 currentPos = glm::mix( ghost.startPos, ghost.endPos, ghost.moveT);

    if (&ghost == &ghostWineStorage)
    {
        currentPos.x += 1.5f; // 위치 보정
    }

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, currentPos);
    // 와인창고 유령만 y축 회전
    if (&ghost == &ghostWineStorage)
    {
        model = glm::translate(model, glm::vec3(-0.1f, 0.0f, 0.0f));

        model = glm::rotate(
            model,
            glm::radians(90.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)
        );

        model = glm::translate(model, glm::vec3(0.1f, 0.0f, 0.0f));
    }

    model = glm::scale(model, glm::vec3(0.47f));

    ghostModel.Draw(shader, model);
}

// 유령의 위치가 플레이어 위치와 달라 보정하는 함수
glm::vec3 FixGhostPos(glm::vec3 pos)
{
    pos.y += -1.2f;
    pos.z += 2.8f;
    return pos;
}