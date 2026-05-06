#ifndef MODEL_H
#define MODEL_H

#include <glad/glad.h> 

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stb_image.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <learnopengl/mesh.h>
#include <learnopengl/shader.h>

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <vector>
using namespace std;

unsigned int TextureFromFile(const char *path, const string &directory, bool gamma = false);

struct ModelNode {
    string name;  // Object_79 같은 node/object 이름
    vector<unsigned int> meshIndices;  // 해당 노드가 가지는 mesh 목록
    glm::mat4 transform;  // 이 node의 누적 transform -> object의 정확한 위치를 찾기 위함
};

// 이전 코드: node를 순회하면서 mesh를 생성함
// 수정 후 코드: mesh를 먼저 생성한 후, node는 어떤 mesh를 쓰는지만 저장함
class Model 
{
public:
    /* model data
       
       gltf에선 계층 구조를 가짐
         scene (model)
        └─ node(object)
           ├─ transform
           └─ mesh 참조
              └─ material
    */
    vector<Texture> textures_loaded;	// stores all the textures loaded so far, optimization to make sure textures aren't loaded more than once.
    vector<Mesh>    meshes;
    vector<ModelNode> nodes;  // 노드 정보
    vector<MaterialInfo> materials;  // 재료 정보

    string directory;
    bool gammaCorrection;

    // constructor, expects a filepath to a 3D model.
    Model(string const &path, bool gamma = false) : gammaCorrection(gamma)
    {
        loadModel(path);
    }

    // draws the model. 하지만 기존과 다르게 mesh들을 돌면서 그리는 것이 아니라 node를 돌면서 그림
    void Draw(Shader &shader, glm::mat4 modelMatrix = glm::mat4(1.0f))
    {
        for (unsigned int i = 0; i < nodes.size(); i++) {
            ModelNode& node = nodes[i];

            // 해당 노드의 좌표에 누적 transform 매트릭스를 곱해서 월드 위치로 이동시킴
            glm::mat4 finalModel = modelMatrix * node.transform;
            shader.setMat4("model", finalModel);

            // node가 가진 모든 mesh를 그림
            for (unsigned int j = 0; j < node.meshIndices.size(); j++) {
                unsigned int meshIndex = node.meshIndices[j];

                Mesh& mesh = meshes[meshIndex];  // 인덱스를 이용하여 mesh 하나를 꺼냄

                bool isEmissive = materials[mesh.materialIndex].emissive;  // 해당 mesh가 사용하는 material이 빛나는지 확인함
                shader.setBool("isEmissive", isEmissive);

                mesh.Draw(shader);
            }
        }
    }

    /* 
        material 이름을 통해 전등의 위치를 찾는 함수->point light의 위치를 구하는데에 사용됨

        Object_79 하나 안에 material_32를 쓰는 Mesh[77]이 있고, 그 mesh 내부에 여러 전등 조각들이 흩어져 있어서
        함수가 mesh 내부 vertex 연결 덩어리를 찾아 각 덩어리 중심을 point light 위치로 쓰는 작업
    */
    vector<glm::vec3> GetLightPositionsFromMaterialParts(string materialName,glm::mat4 modelMatrix) {
        // 1차 후보 위치들로, material_32 mesh 안에서 분리된 작은 조각들의 중심점이 여기에 들어감
        vector<glm::vec3> candidates;

        for (unsigned int n = 0; n < nodes.size(); n++) {
            ModelNode& node = nodes[n];

            // node가 참조하는 모든 mesh를 확인
            for (unsigned int mi = 0; mi < node.meshIndices.size(); mi++) {
                Mesh& mesh = meshes[node.meshIndices[mi]];

                // 원하는 material이 아니면 건너뜀
                if (materials[mesh.materialIndex].name != materialName)
                    continue;

                int vertexCount = mesh.vertices.size();
                vector<vector<int>> graph(vertexCount);

                // 삼각형 index를 이용해서 vertex 연결 그래프 생성
                // 같은 삼각형에 속한 vertex들은 서로 연결된 것으로 봄
                for (unsigned int i = 0; i + 2 < mesh.indices.size(); i += 3) {
                    int a = mesh.indices[i];
                    int b = mesh.indices[i + 1];
                    int c = mesh.indices[i + 2];

                    graph[a].push_back(b);
                    graph[a].push_back(c);

                    graph[b].push_back(a);
                    graph[b].push_back(c);

                    graph[c].push_back(a);
                    graph[c].push_back(b);
                }

                vector<bool> visited(vertexCount, false);

                // 연결된 vertex 덩어리들을 하나씩 찾음
                for (int start = 0; start < vertexCount; start++) {
                    if (visited[start])
                        continue;

                    vector<int> stack;
                    vector<int> component;

                    stack.push_back(start);
                    visited[start] = true;

                    while (!stack.empty()) {
                        int v = stack.back();
                        stack.pop_back();

                        component.push_back(v);

                        for (int next : graph[v]) {
                            if (!visited[next]) {
                                visited[next] = true;
                                stack.push_back(next);
                            }
                        }
                    }

                    // 너무 작은 조각을 무시하고 싶으면 이 값을 3, 4, 8 등으로 조절
                    // 지금은 전등 조각이 작으므로 일단 0으로 둠
                    if (component.size() < 1)
                        continue;

                    // component의 bounding box 계산
                    glm::vec3 minPos(FLT_MAX);
                    glm::vec3 maxPos(-FLT_MAX);

                    for (int idx : component) {
                        glm::vec3 p = mesh.vertices[idx].Position;

                        minPos = glm::min(minPos, p);
                        maxPos = glm::max(maxPos, p);
                    }

                    // component의 로컬 중심
                    glm::vec3 localCenter = (minPos + maxPos) * 0.5f;

                    // node transform + 모델 전체 transform을 적용해서 월드 위치로 변환
                    glm::mat4 worldMatrix = modelMatrix * node.transform;
                    glm::vec3 worldCenter =
                        glm::vec3(worldMatrix * glm::vec4(localCenter, 1.0f));

                    candidates.push_back(worldCenter);
                }
            }
        }

        // 가까운 후보들을 하나의 point light 위치로 합침
        vector<glm::vec3> mergedPositions;
        vector<int> mergedCounts;

        // 값이 클수록 더 많이 합쳐짐
        // 너무 많이 생기면 0.8 ~ 1.2
        // 너무 적게 생기면 0.3 ~ 0.5
        float mergeDistance = 3.0f;

        for (glm::vec3& p : candidates) {
            bool merged = false;

            for (unsigned int i = 0; i < mergedPositions.size(); i++) {
                if (glm::distance(mergedPositions[i], p) < mergeDistance) {
                    // 평균 위치 갱신
                    mergedPositions[i] = (mergedPositions[i] * (float)mergedCounts[i] + p) / (float)(mergedCounts[i] + 1);
                    mergedCounts[i]++;
                    merged = true;

                    break;
                }
            }

            if (!merged) {
                mergedPositions.push_back(p);
                mergedCounts.push_back(1);
            }
        }

        return mergedPositions;
    }
    
private:
    // loads a model with supported ASSIMP extensions from file and stores the resulting meshes in the meshes vector.
    void loadModel(string const &path)
    {
        // read file via ASSIMP
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace);
        // check for errors
        if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) // if is Not Zero
        {
            cout << "ERROR::ASSIMP:: " << importer.GetErrorString() << endl;
            return;
        }
        // retrieve the directory path of the filepath
        directory = path.substr(0, path.find_last_of('/'));

        loadMaterials(scene); // 머테리얼 정보들을 로드함

        // 모든 mesh 정보들을 로드함
        for (unsigned int i = 0; i < scene->mNumMeshes; i++)
        {
            meshes.push_back(processMesh(scene->mMeshes[i], scene));
        }

        // process ASSIMP's root node recursively
        processNode(scene->mRootNode, scene, glm::mat4(1.0f));  // gltf가 세상이 반대로 뒤집혀 있으므로 부모의 위치를 자식에게 반영해야 함

        // 어떤 object가 어떤 mesh를 쓰는지, 각 mesh가 어떤 material을 쓰는지 출력함
        cout << "--- Nodes ---" << endl;
        for (unsigned int i = 0; i < nodes.size(); i++)
        {
            cout << "Node[" << i << "] "
                << nodes[i].name << endl;

            for (unsigned int j = 0; j < nodes[i].meshIndices.size(); j++)
            {
                unsigned int meshIndex =
                    nodes[i].meshIndices[j];

                Mesh& mesh = meshes[meshIndex];

                cout << "    -> Mesh[" << meshIndex << "] "
                    << mesh.name
                    << " | Material: "
                    << materials[mesh.materialIndex].name
                    << endl;
            }
        }
    }

    // material 정보들을 저장하는 함수이다.
    void loadMaterials(const aiScene* scene)
    {
        materials.clear();

        for (unsigned int i = 0; i < scene->mNumMaterials; i++)
        {
            aiMaterial* mat = scene->mMaterials[i];

            aiString matName;
            mat->Get(AI_MATKEY_NAME, matName);

            MaterialInfo info;
            info.name = matName.C_Str();
            info.index = i;
            info.emissive = false;

            // 32번 material에 대해 전등으로 취급함
            if (info.name == "material_32")
            {
                info.emissive = true;
            }

            materials.push_back(info);

            cout << "Material[" << i << "] " << info.name << endl;
        }
    }

    // mesh를 새로 만들지 않고, node가 어떤 mesh를 참조하는지만 저장함
    void processNode(aiNode *node, const aiScene *scene, glm::mat4 parentTrans)  // 인자에 부모 행렬 추가
    {
        // assimp 행렬을 glm 행렬로 변경함 (열 우선 방식으로 변경)
        glm::mat4 nodeTrans = convertToGlmMatrix(node->mTransformation);  // 현재 부모로부터의 거리를 가진 행렬

        // 부모의 위치와 현재 위치를 합쳐 월드 좌표로 변경함
        glm::mat4 totalTrans = parentTrans * nodeTrans;

        // node의 이름, 행렬 정보를 저장함
        ModelNode modelNode;
        modelNode.name = node->mName.C_Str();
        modelNode.transform = totalTrans;

        // node가 가진 mesh의 인덱스를 저장함
        for(unsigned int i = 0; i < node->mNumMeshes; i++)
        {
            modelNode.meshIndices.push_back(node->mMeshes[i]);
        }
        nodes.push_back(modelNode);  // 현재 노드를 저장함

        // after we've processed all of the meshes (if any) we then recursively process each of the children nodes
        for(unsigned int i = 0; i < node->mNumChildren; i++)
        {
            processNode(node->mChildren[i], scene, totalTrans);
        }

    }

    // 행 우선 행렬을 열 우선 행렬로 변환하는 함수 (mat4에만 대응)
    glm::mat4 convertToGlmMatrix(const aiMatrix4x4& from)
    {
        glm::mat4 to;
        // Assimp의 행 요소를 GLM의 열 요소로 정확하게 배정합니다.
        to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
        to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
        to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
        to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
        return to;
    }

    Mesh processMesh(aiMesh *mesh, const aiScene *scene)
    {
        string mName = string(mesh->mName.C_Str());  // assimp로부터 이름을 추출한다.

        // data to fill
        vector<Vertex> vertices;
        vector<unsigned int> indices;
        vector<Texture> textures;

        // walk through each of the mesh's vertices
        for(unsigned int i = 0; i < mesh->mNumVertices; i++)
        {
            Vertex vertex;
            glm::vec3 vector; // we declare a placeholder vector since assimp uses its own vector class that doesn't directly convert to glm's vec3 class so we transfer the data to this placeholder glm::vec3 first.
            // positions
            vector.x = mesh->mVertices[i].x;
            vector.y = mesh->mVertices[i].y;
            vector.z = mesh->mVertices[i].z;

            vertex.Position = vector;

            // 전달받은 parentTrans(누적 행렬)를 정점 위치에 곱해줍니다.
            // vec3를 vec4로 변환(w=1.0)하여 곱한 뒤 다시 vec3로 바꿉니다.
            //glm::vec4 transformedPos = parentTrans * glm::vec4(vector, 1.0f);
            //vertex.Position = glm::vec3(transformedPos);
            
            // normals (법선도 행렬의 영향을 받으므로 같이 처리)
            if (mesh->HasNormals())
            {
                vector.x = mesh->mNormals[i].x;
                vector.y = mesh->mNormals[i].y;
                vector.z = mesh->mNormals[i].z;

                vertex.Normal = vector;

                // 법선 벡터 전용 행렬(Normal Matrix)을 사용하여 변환
                //glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(parentTrans)));
                //vertex.Normal = normalMatrix * vector;
            }
            // texture coordinates
            if(mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
            {
                glm::vec2 vec;
                // a vertex can contain up to 8 different texture coordinates. We thus make the assumption that we won't 
                // use models where a vertex can have multiple texture coordinates so we always take the first set (0).
                vec.x = mesh->mTextureCoords[0][i].x; 
                vec.y = mesh->mTextureCoords[0][i].y;
                vertex.TexCoords = vec;
                // tangent
                vector.x = mesh->mTangents[i].x;
                vector.y = mesh->mTangents[i].y;
                vector.z = mesh->mTangents[i].z;
                vertex.Tangent = vector;
                // bitangent
                vector.x = mesh->mBitangents[i].x;
                vector.y = mesh->mBitangents[i].y;
                vector.z = mesh->mBitangents[i].z;
                vertex.Bitangent = vector;
            }
            else
                vertex.TexCoords = glm::vec2(0.0f, 0.0f);

            vertices.push_back(vertex);
        }
        // now wak through each of the mesh's faces (a face is a mesh its triangle) and retrieve the corresponding vertex indices.
        for(unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];
            // retrieve all indices of the face and store them in the indices vector
            for(unsigned int j = 0; j < face.mNumIndices; j++)
                indices.push_back(face.mIndices[j]);        
        }
        // process materials
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];    
        // we assume a convention for sampler names in the shaders. Each diffuse texture should be named
        // as 'texture_diffuseN' where N is a sequential number ranging from 1 to MAX_SAMPLER_NUMBER. 
        // Same applies to other texture as the following list summarizes:
        // diffuse: texture_diffuseN
        // specular: texture_specularN
        // normal: texture_normalN

        // 1. diffuse maps
        vector<Texture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse");
        textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
        // 2. specular maps
        vector<Texture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
        textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
        // 3. normal maps
        std::vector<Texture> normalMaps = loadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal");
        textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
        // 4. height maps
        std::vector<Texture> heightMaps = loadMaterialTextures(material, aiTextureType_AMBIENT, "texture_height");
        textures.insert(textures.end(), heightMaps.begin(), heightMaps.end());
        
        // return a mesh object created from the extracted mesh data
        return Mesh(mName, mesh->mMaterialIndex, vertices, indices, textures);
    }

    // checks all material textures of a given type and loads the textures if they're not loaded yet.
    // the required info is returned as a Texture struct.
    vector<Texture> loadMaterialTextures(aiMaterial *mat, aiTextureType type, string typeName)
    {
        vector<Texture> textures;
        for(unsigned int i = 0; i < mat->GetTextureCount(type); i++)
        {
            aiString str;
            mat->GetTexture(type, i, &str);
            // check if texture was loaded before and if so, continue to next iteration: skip loading a new texture
            bool skip = false;
            for(unsigned int j = 0; j < textures_loaded.size(); j++)
            {
                if(std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0)
                {
                    textures.push_back(textures_loaded[j]);
                    skip = true; // a texture with the same filepath has already been loaded, continue to next one. (optimization)
                    break;
                }
            }
            if(!skip)
            {   // if texture hasn't been loaded already, load it
                Texture texture;
                texture.id = TextureFromFile(str.C_Str(), this->directory);
                texture.type = typeName;
                texture.path = str.C_Str();
                textures.push_back(texture);
                textures_loaded.push_back(texture);  // store it as texture loaded for entire model, to ensure we won't unnecesery load duplicate textures.
            }
        }
        return textures;
    }
};


unsigned int TextureFromFile(const char *path, const string &directory, bool gamma)
{
    string filename = string(path);
    filename = directory + '/' + filename;

    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}
#endif
