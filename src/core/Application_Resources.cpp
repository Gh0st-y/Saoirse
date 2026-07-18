// Mesh/texture loading and per-GameObject GPU resources
//
// Part of the Application class split. See Application.h for the full
// class declaration - all methods below are Application:: members.
#include "Application.h"
#include <iostream>
#include <stb_image.h>


// --- Load Mesh ---
Mesh* Application::loadMesh(const std::string& path) {
    auto mesh = std::make_unique<Mesh>();

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals | aiProcess_CalcTangentSpace);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        throw std::runtime_error(std::string("Assimp failed to load model: ") + importer.GetErrorString());
    }

    aiMesh* aMesh = scene->mMeshes[0];   // still single-mesh-per-file for now — Section 11 properly addresses multi-mesh files

    for (unsigned int i = 0; i < aMesh->mNumVertices; i++) {
        Vertex vertex{};
        vertex.pos = { aMesh->mVertices[i].x, aMesh->mVertices[i].y, aMesh->mVertices[i].z };

        if (aMesh->HasNormals()) {
            vertex.normal = { aMesh->mNormals[i].x, aMesh->mNormals[i].y, aMesh->mNormals[i].z };
        }
        else {
            vertex.normal = { 0.0f, 0.0f, 1.0f };
        }

        if (aMesh->HasTangentsAndBitangents())
        {
            vertex.tangent = { aMesh->mTangents[i].x, aMesh->mTangents[i].y, aMesh->mTangents[i].z };
        }
        else {
            vertex.tangent = { 1.0f, 0.0f, 0.0f };
        }

        if (aMesh->mTextureCoords[0]) {
            vertex.texCoord = { aMesh->mTextureCoords[0][i].x, aMesh->mTextureCoords[0][i].y };
        }
        else {
            vertex.texCoord = { 0.0f, 0.0f };
        }

        vertex.color = { 1.0f, 1.0f, 1.0f };
        mesh->vertices.push_back(vertex);
    }

    for (unsigned int i = 0; i < aMesh->mNumFaces; i++) {
        aiFace face = aMesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            mesh->indices.push_back(face.mIndices[j]);
        }
    }

    mesh->computeBoundingSphere();

    createMeshBuffers(mesh.get());

    std::cout << "Loaded mesh '" << path << "': " << mesh->vertices.size()
        << " vertices, " << mesh->indices.size() / 3 << " triangles, bounding radius "
        << mesh->boundingSphereRadius << std::endl;

    meshes.push_back(std::move(mesh));
    return meshes.back().get();
}


// --- Load Texture ---
Texture* Application::loadTexture(const std::string& path) {
    auto texture = std::make_unique<Texture>();

    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

    if (!pixels) {
        throw std::runtime_error("failed to load texture image: " + path);
    }

    VkDeviceSize imageSize = texWidth * texHeight * 4;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    vulkanDevice.createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(vulkanDevice.getDevice(), stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(vulkanDevice.getDevice(), stagingBufferMemory);

    stbi_image_free(pixels);

    vulkanDevice.createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture->image, texture->imageMemory);

    vulkanDevice.transitionImageLayout(texture->image, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vulkanDevice.copyBufferToImage(stagingBuffer, texture->image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
    vulkanDevice.transitionImageLayout(texture->image, VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(vulkanDevice.getDevice(), stagingBuffer, nullptr);
    vkFreeMemory(vulkanDevice.getDevice(), stagingBufferMemory, nullptr);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = texture->image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(vulkanDevice.getDevice(), &viewInfo, nullptr, &texture->imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture image view!");
    }

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(vulkanDevice.getPhysicalDevice(), &properties);
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(vulkanDevice.getDevice(), &samplerInfo, nullptr, &texture->sampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }

    textures.push_back(std::move(texture));
    return textures.back().get();
}


// --- Create Mesh Buffer ---
void Application::createMeshBuffers(Mesh* mesh) {
    // Vertex buffer
    VkDeviceSize vertexBufferSize = sizeof(mesh->vertices[0]) * mesh->vertices.size();

    VkBuffer vStagingBuffer;
    VkDeviceMemory vStagingBufferMemory;
    vulkanDevice.createBuffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        vStagingBuffer, vStagingBufferMemory);

    void* vData;
    vkMapMemory(vulkanDevice.getDevice(), vStagingBufferMemory, 0, vertexBufferSize, 0, &vData);
    memcpy(vData, mesh->vertices.data(), (size_t)vertexBufferSize);
    vkUnmapMemory(vulkanDevice.getDevice(), vStagingBufferMemory);

    vulkanDevice.createBuffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mesh->vertexBuffer, mesh->vertexBufferMemory);

    vulkanDevice.copyBuffer(vStagingBuffer, mesh->vertexBuffer, vertexBufferSize);

    vkDestroyBuffer(vulkanDevice.getDevice(), vStagingBuffer, nullptr);
    vkFreeMemory(vulkanDevice.getDevice(), vStagingBufferMemory, nullptr);

    // Index buffer
    VkDeviceSize indexBufferSize = sizeof(mesh->indices[0]) * mesh->indices.size();

    VkBuffer iStagingBuffer;
    VkDeviceMemory iStagingBufferMemory;
    vulkanDevice.createBuffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        iStagingBuffer, iStagingBufferMemory);

    void* iData;
    vkMapMemory(vulkanDevice.getDevice(), iStagingBufferMemory, 0, indexBufferSize, 0, &iData);
    memcpy(iData, mesh->indices.data(), (size_t)indexBufferSize);
    vkUnmapMemory(vulkanDevice.getDevice(), iStagingBufferMemory);

    vulkanDevice.createBuffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mesh->indexBuffer, mesh->indexBufferMemory);

    vulkanDevice.copyBuffer(iStagingBuffer, mesh->indexBuffer, indexBufferSize);

    vkDestroyBuffer(vulkanDevice.getDevice(), iStagingBuffer, nullptr);
    vkFreeMemory(vulkanDevice.getDevice(), iStagingBufferMemory, nullptr);
}

void Application::destroyMesh(Mesh* mesh) {
    vkDestroyBuffer(vulkanDevice.getDevice(), mesh->indexBuffer, nullptr);
    vkFreeMemory(vulkanDevice.getDevice(), mesh->indexBufferMemory, nullptr);
    vkDestroyBuffer(vulkanDevice.getDevice(), mesh->vertexBuffer, nullptr);
    vkFreeMemory(vulkanDevice.getDevice(), mesh->vertexBufferMemory, nullptr);
}

void Application::destroyTexture(Texture* texture) {
    vkDestroySampler(vulkanDevice.getDevice(), texture->sampler, nullptr);
    vkDestroyImageView(vulkanDevice.getDevice(), texture->imageView, nullptr);
    vkDestroyImage(vulkanDevice.getDevice(), texture->image, nullptr);
    vkFreeMemory(vulkanDevice.getDevice(), texture->imageMemory, nullptr);
}


// --- Create Game Object Resources
void Application::createGameObjectResources(GameObject& obj) {
    VkDeviceSize bufferSize = sizeof(ObjectUBO);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vulkanDevice.createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        obj.uniformBuffers[i], obj.uniformBufferMemories[i]);
        
        vkMapMemory(vulkanDevice.getDevice(), obj.uniformBufferMemories[i], 0, bufferSize, 0, &obj.uniformBuffersMapped[i]);
        
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &objectDescriptorSetLayout;
        
        if (vkAllocateDescriptorSets(vulkanDevice.getDevice(), &allocInfo, &obj.descriptorSets[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor set!");
        }
        
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = obj.uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(ObjectUBO);
        
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = obj.texture->imageView;
        imageInfo.sampler = obj.texture->sampler;

        VkDescriptorImageInfo normalMapInfo{};
        normalMapInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        Texture* normalMap = obj.normalMap ? obj.normalMap : defaultNormalMap;
        normalMapInfo.imageView = normalMap->imageView;
        normalMapInfo.sampler = normalMap->sampler;
        
        std::array<VkWriteDescriptorSet, 3> descriptorWrites{};
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = obj.descriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;
        
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = obj.descriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &imageInfo;

        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = obj.descriptorSets[i];
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pImageInfo = &normalMapInfo;
        
        vkUpdateDescriptorSets(vulkanDevice.getDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
}

// --- Update Game Object Uniform Buffer ---
void Application::updateGameObjectUniformBuffer(GameObject& obj, uint32_t frameIndex) {
    ObjectUBO ubo{};
    ubo.model = obj.getWorldMatrix();
    ubo.normalMatrix = glm::transpose(glm::inverse(ubo.model));
    memcpy(obj.uniformBuffersMapped[frameIndex], &ubo, sizeof(ubo));
}

// --- Create Default Normal Map ---
void Application::createDefaultNormalMap() {
    // (128, 128, 255, 255) = tangent-space "up" vector (0,0,1) encoded into [0,1]
    uint8_t pixels[4] = { 128, 128, 255, 255 };

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    vulkanDevice.createBuffer(4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(vulkanDevice.getDevice(), stagingBufferMemory, 0, 4, 0, &data);
    memcpy(data, pixels, 4);
    vkUnmapMemory(vulkanDevice.getDevice(), stagingBufferMemory);

    auto texture = std::make_unique<Texture>();

    vulkanDevice.createImage(1, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture->image, texture->imageMemory);

    vulkanDevice.transitionImageLayout(texture->image, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vulkanDevice.copyBufferToImage(stagingBuffer, texture->image, 1, 1);
    vulkanDevice.transitionImageLayout(texture->image, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(vulkanDevice.getDevice(), stagingBuffer, nullptr);
    vkFreeMemory(vulkanDevice.getDevice(), stagingBufferMemory, nullptr);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = texture->image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(vulkanDevice.getDevice(), &viewInfo, nullptr, &texture->imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create default normal map image view!");
    }

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    if (vkCreateSampler(vulkanDevice.getDevice(), &samplerInfo, nullptr, &texture->sampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create default normal map sampler!");
    }

    textures.push_back(std::move(texture));
    defaultNormalMap = textures.back().get();
}