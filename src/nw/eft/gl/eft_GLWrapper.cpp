#include <nw/eft/gl/eft_GLWrapper.h>
#include <nw/eft/eft_Heap.h>

#if EFT_IS_PC

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <sstream>

#include "file.hpp"
#include "md5.hpp"

#pragma once

#include <string>
#include <unordered_map>

const std::string g_CWD = std::filesystem::current_path().generic_string();
const std::string g_CafePath = g_CWD + "/Cafe";
const std::string g_CafeCachePath = g_CafePath + "/Cache";

struct ShaderCache
{
    ShaderCache(const std::string& v, const std::string& f)
        : vertexShader(v)
        , fragmentShader(f)
    {
    }

    ~ShaderCache() = default;

    std::string vertexShader;
    std::string fragmentShader;
};

typedef std::unordered_map<std::string, const ShaderCache> ShaderCacheMap;

ShaderCacheMap g_ShaderCache;

static GLuint CompileShader(const char* source, GLenum type)
{
    GLuint id = glCreateShader(type);
    glShaderSource(id, 1, &source, NULL);
    glCompileShader(id);

    GLint result;
    glGetShaderiv(id, GL_COMPILE_STATUS, &result);
    if (result == GL_FALSE)
    {
        GLint length;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);

        char* message = new char[length];
        glGetShaderInfoLog(id, length, &length, message);
        printf("Failed to compile %s shader\n", type == GL_VERTEX_SHADER ? "vertex" : "fragment");
        printf("%s\n", message);
        delete[] message;

        glDeleteShader(id);
        return GL_NONE;
    }

    return id;
}

static GLuint CompileProgram(const char* vertexShader,
                             const char* fragmentShader)
{
    GLuint program = glCreateProgram();
    GLuint vs = CompileShader(vertexShader, GL_VERTEX_SHADER);
    GLuint fs = CompileShader(fragmentShader, GL_FRAGMENT_SHADER);

    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glValidateProgram(program);

    GLint result;
    glGetProgramiv(program, GL_LINK_STATUS, &result);
    if (result == GL_FALSE)
    {
        GLint length;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);

        char* message = new char[length];
        glGetProgramInfoLog(program, length, &length, message);
        printf("Failed to link program\n");
        printf("%s\n", message);
        delete[] message;

        glDeleteProgram(program);

        glDeleteShader(vs);
        glDeleteShader(fs);

        return GL_NONE;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    return program;
}

static size_t SaveGX2VertexShader(nw::eft::Heap* heap,
                                  GX2VertexShader* shader,
                                  void** pShaderBuf)
{
    size_t shaderBufSize = sizeof(GX2VertexShader);

    const size_t uniformBlocksOffs = shaderBufSize;
    shaderBufSize += shader->numUniformBlocks * sizeof(GX2UniformBlock);

    const size_t uniformVarsOffs = shaderBufSize;
    shaderBufSize += shader->numUniforms * sizeof(GX2UniformVar);

    const size_t initialValuesOffs = shaderBufSize;
    shaderBufSize += shader->numInitialValues * sizeof(GX2UniformInitialValue);

    const size_t loopVarsOffs = shaderBufSize;
    shaderBufSize += shader->_numLoops * sizeof(u32) * 2;

    const size_t samplerVarsOffs = shaderBufSize;
    shaderBufSize += shader->numSamplers * sizeof(GX2SamplerVar);

    const size_t attribVarsOffs = shaderBufSize;
    shaderBufSize += shader->numAttribs * sizeof(GX2AttribVar);

    const size_t strBaseOffs = shaderBufSize;

    for (u32 i = 0; i < shader->numUniformBlocks; i++)
    {
        const char* const name = shader->uniformBlocks.getIndexed(i)->name.get();
        if (name != NULL)
            shaderBufSize += std::strlen(name) + 1;
    }

    for (u32 i = 0; i < shader->numUniforms; i++)
    {
        const char* const name = shader->uniformVars.getIndexed(i)->name.get();
        if (name != NULL)
            shaderBufSize += std::strlen(name) + 1;
    }

    for (u32 i = 0; i < shader->numSamplers; i++)
    {
        const char* const name = shader->samplerVars.getIndexed(i)->name.get();
        if (name != NULL)
            shaderBufSize += std::strlen(name) + 1;
    }

    for (u32 i = 0; i < shader->numAttribs; i++)
    {
        const char* const name = shader->attribVars.getIndexed(i)->name.get();
        if (name != NULL)
            shaderBufSize += std::strlen(name) + 1;
    }

    const size_t shaderDataOffs = shaderBufSize;
    shaderBufSize += shader->shaderSize;

    u8* shaderBuf = (u8*)heap->Alloc(shaderBufSize);

    std::memcpy(shaderBuf, shader, sizeof(GX2VertexShader));
    std::memcpy(shaderBuf + shaderDataOffs, shader->shaderPtr.get(), shader->shaderSize);

    *(u32*)(shaderBuf + offsetof(GX2VertexShader, shaderPtr)) = shaderDataOffs;

    if (shader->numUniformBlocks != 0)
    {
        std::memcpy(shaderBuf + uniformBlocksOffs,
                    shader->uniformBlocks.get(),
                    shader->numUniformBlocks * sizeof(GX2UniformBlock));
    }

    *(u32*)(shaderBuf + offsetof(GX2VertexShader, uniformBlocks)) = uniformBlocksOffs;

    if (shader->numUniforms != 0)
    {
        std::memcpy(shaderBuf + uniformVarsOffs,
                    shader->uniformVars.get(),
                    shader->numUniforms * sizeof(GX2UniformVar));
    }

    *(u32*)(shaderBuf + offsetof(GX2VertexShader, uniformVars)) = uniformVarsOffs;

    if (shader->numInitialValues != 0)
    {
        std::memcpy(shaderBuf + initialValuesOffs,
                    shader->initialValues.get(),
                    shader->numInitialValues * sizeof(GX2UniformInitialValue));
    }

    *(u32*)(shaderBuf + offsetof(GX2VertexShader, initialValues)) = initialValuesOffs;

    if (shader->_numLoops != 0)
    {
        std::memcpy(shaderBuf + loopVarsOffs,
                    shader->_loopVars.get(),
                    shader->_numLoops * sizeof(u32) * 2);
    }

    *(u32*)(shaderBuf + offsetof(GX2VertexShader, _loopVars)) = loopVarsOffs;

    if (shader->numSamplers != 0)
    {
        std::memcpy(shaderBuf + samplerVarsOffs,
                    shader->samplerVars.get(),
                    shader->numSamplers * sizeof(GX2SamplerVar));
    }

    *(u32*)(shaderBuf + offsetof(GX2VertexShader, samplerVars)) = samplerVarsOffs;

    if (shader->numAttribs != 0)
    {
        std::memcpy(shaderBuf + attribVarsOffs,
                    shader->attribVars.get(),
                    shader->numAttribs * sizeof(GX2AttribVar));
    }

    *(u32*)(shaderBuf + offsetof(GX2VertexShader, attribVars)) = attribVarsOffs;

    size_t strOffs = strBaseOffs;

    for (u32 i = 0; i < shader->numUniformBlocks; i++)
    {
        const char* const name = shader->uniformBlocks.getIndexed(i)->name.get();
        if (name != NULL)
        {
            const size_t nameLen = std::strlen(name) + 1;
            std::memcpy(shaderBuf + strOffs, name, nameLen);

            *(u32*)(shaderBuf +
                    uniformBlocksOffs
                    + i * sizeof(GX2UniformBlock)
                    + offsetof(GX2UniformBlock, name)) = strOffs;

            strOffs += nameLen;
        }
    }

    for (u32 i = 0; i < shader->numUniforms; i++)
    {
        const char* const name = shader->uniformVars.getIndexed(i)->name.get();
        if (name != NULL)
        {
            const size_t nameLen = std::strlen(name) + 1;
            std::memcpy(shaderBuf + strOffs, name, nameLen);

            *(u32*)(shaderBuf +
                    uniformVarsOffs
                    + i * sizeof(GX2UniformVar)
                    + offsetof(GX2UniformVar, name)) = strOffs;

            strOffs += nameLen;
        }
    }

    for (u32 i = 0; i < shader->numSamplers; i++)
    {
        const char* const name = shader->samplerVars.getIndexed(i)->name.get();
        if (name != NULL)
        {
            const size_t nameLen = std::strlen(name) + 1;
            std::memcpy(shaderBuf + strOffs, name, nameLen);

            *(u32*)(shaderBuf +
                    samplerVarsOffs
                    + i * sizeof(GX2SamplerVar)
                    + offsetof(GX2SamplerVar, name)) = strOffs;

            strOffs += nameLen;
        }
    }

    for (u32 i = 0; i < shader->numAttribs; i++)
    {
        const char* const name = shader->attribVars.getIndexed(i)->name.get();
        if (name != NULL)
        {
            const size_t nameLen = std::strlen(name) + 1;
            std::memcpy(shaderBuf + strOffs, name, nameLen);

            *(u32*)(shaderBuf +
                    attribVarsOffs
                    + i * sizeof(GX2AttribVar)
                    + offsetof(GX2AttribVar, name)) = strOffs;

            strOffs += nameLen;
        }
    }

    *pShaderBuf = shaderBuf;
    return shaderBufSize;
}

static size_t SaveGX2PixelShader(nw::eft::Heap* heap,
                                 GX2PixelShader* shader,
                                 void** pShaderBuf)
{
    size_t shaderBufSize = sizeof(GX2PixelShader);

    const size_t uniformBlocksOffs = shaderBufSize;
    shaderBufSize += shader->numUniformBlocks * sizeof(GX2UniformBlock);

    const size_t uniformVarsOffs = shaderBufSize;
    shaderBufSize += shader->numUniforms * sizeof(GX2UniformVar);

    const size_t initialValuesOffs = shaderBufSize;
    shaderBufSize += shader->numInitialValues * sizeof(GX2UniformInitialValue);

    const size_t loopVarsOffs = shaderBufSize;
    shaderBufSize += shader->_numLoops * sizeof(u32) * 2;

    const size_t samplerVarsOffs = shaderBufSize;
    shaderBufSize += shader->numSamplers * sizeof(GX2SamplerVar);

    const size_t strBaseOffs = shaderBufSize;

    for (u32 i = 0; i < shader->numUniformBlocks; i++)
    {
        const char* const name = shader->uniformBlocks.getIndexed(i)->name.get();
        if (name != NULL)
            shaderBufSize += std::strlen(name) + 1;
    }

    for (u32 i = 0; i < shader->numUniforms; i++)
    {
        const char* const name = shader->uniformVars.getIndexed(i)->name.get();
        if (name != NULL)
            shaderBufSize += std::strlen(name) + 1;
    }

    for (u32 i = 0; i < shader->numSamplers; i++)
    {
        const char* const name = shader->samplerVars.getIndexed(i)->name.get();
        if (name != NULL)
            shaderBufSize += std::strlen(name) + 1;
    }

    const size_t shaderDataOffs = shaderBufSize;
    shaderBufSize += shader->shaderSize;

    u8* shaderBuf = (u8*)heap->Alloc(shaderBufSize);

    std::memcpy(shaderBuf, shader, sizeof(GX2PixelShader));
    std::memcpy(shaderBuf + shaderDataOffs, shader->shaderPtr.get(), shader->shaderSize);

    *(u32*)(shaderBuf + offsetof(GX2PixelShader, shaderPtr)) = shaderDataOffs;

    if (shader->numUniformBlocks != 0)
    {
        std::memcpy(shaderBuf + uniformBlocksOffs,
                    shader->uniformBlocks.get(),
                    shader->numUniformBlocks * sizeof(GX2UniformBlock));
    }

    *(u32*)(shaderBuf + offsetof(GX2PixelShader, uniformBlocks)) = uniformBlocksOffs;

    if (shader->numUniforms != 0)
    {
        std::memcpy(shaderBuf + uniformVarsOffs,
                    shader->uniformVars.get(),
                    shader->numUniforms * sizeof(GX2UniformVar));
    }

    *(u32*)(shaderBuf + offsetof(GX2PixelShader, uniformVars)) = uniformVarsOffs;

    if (shader->numInitialValues != 0)
    {
        std::memcpy(shaderBuf + initialValuesOffs,
                    shader->initialValues.get(),
                    shader->numInitialValues * sizeof(GX2UniformInitialValue));
    }

    *(u32*)(shaderBuf + offsetof(GX2PixelShader, initialValues)) = initialValuesOffs;

    if (shader->_numLoops != 0)
    {
        std::memcpy(shaderBuf + loopVarsOffs,
                    shader->_loopVars.get(),
                    shader->_numLoops * sizeof(u32) * 2);
    }

    *(u32*)(shaderBuf + offsetof(GX2PixelShader, _loopVars)) = loopVarsOffs;

    if (shader->numSamplers != 0)
    {
        std::memcpy(shaderBuf + samplerVarsOffs,
                    shader->samplerVars.get(),
                    shader->numSamplers * sizeof(GX2SamplerVar));
    }

    *(u32*)(shaderBuf + offsetof(GX2PixelShader, samplerVars)) = samplerVarsOffs;

    size_t strOffs = strBaseOffs;

    for (u32 i = 0; i < shader->numUniformBlocks; i++)
    {
        const char* const name = shader->uniformBlocks.getIndexed(i)->name.get();
        if (name != NULL)
        {
            const size_t nameLen = std::strlen(name) + 1;
            std::memcpy(shaderBuf + strOffs, name, nameLen);

            *(u32*)(shaderBuf +
                    uniformBlocksOffs
                    + i * sizeof(GX2UniformBlock)
                    + offsetof(GX2UniformBlock, name)) = strOffs;

            strOffs += nameLen;
        }
    }

    for (u32 i = 0; i < shader->numUniforms; i++)
    {
        const char* const name = shader->uniformVars.getIndexed(i)->name.get();
        if (name != NULL)
        {
            const size_t nameLen = std::strlen(name) + 1;
            std::memcpy(shaderBuf + strOffs, name, nameLen);

            *(u32*)(shaderBuf +
                    uniformVarsOffs
                    + i * sizeof(GX2UniformVar)
                    + offsetof(GX2UniformVar, name)) = strOffs;

            strOffs += nameLen;
        }
    }

    for (u32 i = 0; i < shader->numSamplers; i++)
    {
        const char* const name = shader->samplerVars.getIndexed(i)->name.get();
        if (name != NULL)
        {
            const size_t nameLen = std::strlen(name) + 1;
            std::memcpy(shaderBuf + strOffs, name, nameLen);

            *(u32*)(shaderBuf +
                    samplerVarsOffs
                    + i * sizeof(GX2SamplerVar)
                    + offsetof(GX2SamplerVar, name)) = strOffs;

            strOffs += nameLen;
        }
    }

    *pShaderBuf = shaderBuf;
    return shaderBufSize;
}


static bool GX2UniformBlockComp(const GX2UniformBlock& a, const GX2UniformBlock& b)
{
    return a.location > b.location;
}

static void ReplaceString(std::string& str, const std::string& a, const std::string& b)
{
    size_t pos = 0;
    while ((pos = str.find(a, pos)) != std::string::npos)
    {
         str.replace(pos, a.length(), b);
         pos += b.length();
    }
}

static void DecompileProgram(nw::eft::Heap* heap,
                             GX2VertexShader* vertexShader,
                             GX2PixelShader* pixelShader,
                             std::string* vertexShaderSrc,
                             std::string* fragmentShaderSrc)
{
    void* vertexShaderBuf;
    size_t vertexShaderBufSize = SaveGX2VertexShader(heap, vertexShader, &vertexShaderBuf);

    void* pixelShaderBuf;
    size_t pixelShaderBufSize = SaveGX2PixelShader(heap, pixelShader, &pixelShaderBuf);

    u8* shaderBuf = (u8*)heap->Alloc(vertexShaderBufSize + pixelShaderBufSize);
    std::memcpy(shaderBuf, vertexShaderBuf, vertexShaderBufSize);
    std::memcpy(shaderBuf + vertexShaderBufSize, pixelShaderBuf, pixelShaderBufSize);

    std::string key = md5(shaderBuf, vertexShaderBufSize + pixelShaderBufSize);

    std::string glVertexShader;
    std::string glFragmentShader;

    ShaderCacheMap::const_iterator it = g_ShaderCache.find(key);
    if (it != g_ShaderCache.end())
    {
        const ShaderCache& shaderCache = it->second;
        glVertexShader = shaderCache.vertexShader;
        glFragmentShader = shaderCache.fragmentShader;
    }
    else
    {
        const std::string basePath = g_CafeCachePath + '/' + key;

        const std::string vertexShaderPath = basePath + "VS";
        const std::string fragmentShaderPath = basePath + "FS";

        const std::string vertexShaderSrcPath = vertexShaderPath + ".vert";
        const std::string fragmentShaderSrcPath = fragmentShaderPath + ".frag";

        if (FileExists(vertexShaderSrcPath.c_str()) && FileExists(fragmentShaderSrcPath.c_str()))
        {
            bool success = ReadFile(vertexShaderSrcPath, &glVertexShader);
            assert(success);

            success = ReadFile(fragmentShaderSrcPath, &glFragmentShader);
            assert(success);
        }
        else
        {
            const std::string vertexShaderSpirvPath = vertexShaderSrcPath + ".spv";
            const std::string fragmentShaderSpirvPath = fragmentShaderSrcPath + ".spv";

            printf("\n");
            //printf("%s\n", vertexShaderSpirvPath.c_str());
            //printf("%s\n", fragmentShaderSpirvPath.c_str());

            WriteFile(vertexShaderPath.c_str(), (u8*)vertexShaderBuf, vertexShaderBufSize);
            WriteFile(fragmentShaderPath.c_str(), (u8*)pixelShaderBuf, pixelShaderBufSize);

            std::string cmd;

            {
                std::ostringstream cmdStrm;
#ifndef NW_PLATFORM_WIN32
                cmdStrm << "wine ";
#endif
                cmdStrm << "\"" << g_CafePath << "/gx2shader-decompiler.exe\" -v \"" << vertexShaderPath << "\" -p \"" << fragmentShaderPath << "\"";
                cmd = cmdStrm.str();
            }

            printf("%s\n", cmd.c_str());
            system(cmd.c_str());

            bool success = FileExists(vertexShaderSpirvPath.c_str());
            assert(success);

            success = FileExists(fragmentShaderSpirvPath.c_str());
            assert(success);

            DeleteFile(vertexShaderPath.c_str());
            DeleteFile(fragmentShaderPath.c_str());

            {
                std::ostringstream cmdStrm;
#ifndef NW_PLATFORM_WIN32
                cmdStrm << "wine ";
#endif
                cmdStrm << "\"" << g_CafePath << "/spirv-cross.exe\" \"" << vertexShaderSpirvPath << "\" --no-es "  \
                "--no-420pack-extension --no-support-nonzero-baseinstance " \
                "--rename-interface-variable out 0 PARAM_0 " \
                "--rename-interface-variable out 1 PARAM_1 " \
                "--rename-interface-variable out 2 PARAM_2 " \
                "--rename-interface-variable out 3 PARAM_3 " \
                "--rename-interface-variable out 4 PARAM_4 " \
                "--rename-interface-variable out 5 PARAM_5 " \
                "--rename-interface-variable out 6 PARAM_6 " \
                "--rename-interface-variable out 7 PARAM_7 " \
                "--rename-interface-variable out 8 PARAM_8 " \
                "--rename-interface-variable out 9 PARAM_9 " \
                "--rename-interface-variable out 10 PARAM_10 " \
                "--rename-interface-variable out 11 PARAM_11 " \
                "--rename-interface-variable out 12 PARAM_12 " \
                "--rename-interface-variable out 13 PARAM_13 " \
                "--rename-interface-variable out 14 PARAM_14 " \
                "--rename-interface-variable out 15 PARAM_15 " \
                "--rename-interface-variable out 16 PARAM_16 " \
                "--rename-interface-variable out 17 PARAM_17 " \
                "--rename-interface-variable out 18 PARAM_18 " \
                "--rename-interface-variable out 19 PARAM_19 " \
                "--rename-interface-variable out 20 PARAM_20 " \
                "--rename-interface-variable out 21 PARAM_21 " \
                "--rename-interface-variable out 22 PARAM_22 " \
                "--rename-interface-variable out 23 PARAM_23 " \
                "--rename-interface-variable out 24 PARAM_24 " \
                "--rename-interface-variable out 25 PARAM_25 " \
                "--rename-interface-variable out 26 PARAM_26 " \
                "--rename-interface-variable out 27 PARAM_27 " \
                "--rename-interface-variable out 28 PARAM_28 " \
                "--rename-interface-variable out 29 PARAM_29 " \
                "--rename-interface-variable out 30 PARAM_30 " \
                "--rename-interface-variable out 31 PARAM_31 " \
                "--rename-interface-variable out 32 PARAM_32 " \
                "--rename-interface-variable out 33 PARAM_33 " \
                "--rename-interface-variable out 34 PARAM_34 " \
                "--rename-interface-variable out 35 PARAM_35 " \
                "--rename-interface-variable out 36 PARAM_36 " \
                "--rename-interface-variable out 37 PARAM_37 " \
                "--rename-interface-variable out 38 PARAM_38 " \
                "--rename-interface-variable out 39 PARAM_39 " \
                "--rename-interface-variable out 40 PARAM_40 " \
                "--rename-interface-variable out 41 PARAM_41 " \
                "--rename-interface-variable out 42 PARAM_42 " \
                "--rename-interface-variable out 43 PARAM_43 " \
                "--rename-interface-variable out 44 PARAM_44 " \
                "--rename-interface-variable out 45 PARAM_45 " \
                "--rename-interface-variable out 46 PARAM_46 " \
                "--rename-interface-variable out 47 PARAM_47 " \
                "--rename-interface-variable out 48 PARAM_48 " \
                "--rename-interface-variable out 49 PARAM_49 " \
                "--rename-interface-variable out 50 PARAM_50 " \
                "--rename-interface-variable out 51 PARAM_51 " \
                "--rename-interface-variable out 52 PARAM_52 " \
                "--rename-interface-variable out 53 PARAM_53 " \
                "--rename-interface-variable out 54 PARAM_54 " \
                "--rename-interface-variable out 55 PARAM_55 " \
                "--rename-interface-variable out 56 PARAM_56 " \
                "--rename-interface-variable out 57 PARAM_57 " \
                "--rename-interface-variable out 58 PARAM_58 " \
                "--rename-interface-variable out 59 PARAM_59 " \
                "--rename-interface-variable out 60 PARAM_60 " \
                "--rename-interface-variable out 61 PARAM_61 " \
                "--rename-interface-variable out 62 PARAM_62 " \
                "--rename-interface-variable out 63 PARAM_63 " \
                "--version 330 --output \"" << vertexShaderSrcPath << "\"";
                cmd = cmdStrm.str();
            }

            printf("%s\n", cmd.c_str());
            system(cmd.c_str());

            success = FileExists(vertexShaderSrcPath.c_str());
            assert(success);

            {
                std::ostringstream cmdStrm;
#ifndef NW_PLATFORM_WIN32
                cmdStrm << "wine ";
#endif
                cmdStrm << "\"" << g_CafePath << "/spirv-cross.exe\" \"" << fragmentShaderSpirvPath << "\" --no-es "  \
                "--no-420pack-extension --no-support-nonzero-baseinstance " \
                "--rename-interface-variable in 0 PARAM_0 " \
                "--rename-interface-variable in 1 PARAM_1 " \
                "--rename-interface-variable in 2 PARAM_2 " \
                "--rename-interface-variable in 3 PARAM_3 " \
                "--rename-interface-variable in 4 PARAM_4 " \
                "--rename-interface-variable in 5 PARAM_5 " \
                "--rename-interface-variable in 6 PARAM_6 " \
                "--rename-interface-variable in 7 PARAM_7 " \
                "--rename-interface-variable in 8 PARAM_8 " \
                "--rename-interface-variable in 9 PARAM_9 " \
                "--rename-interface-variable in 10 PARAM_10 " \
                "--rename-interface-variable in 11 PARAM_11 " \
                "--rename-interface-variable in 12 PARAM_12 " \
                "--rename-interface-variable in 13 PARAM_13 " \
                "--rename-interface-variable in 14 PARAM_14 " \
                "--rename-interface-variable in 15 PARAM_15 " \
                "--rename-interface-variable in 16 PARAM_16 " \
                "--rename-interface-variable in 17 PARAM_17 " \
                "--rename-interface-variable in 18 PARAM_18 " \
                "--rename-interface-variable in 19 PARAM_19 " \
                "--rename-interface-variable in 20 PARAM_20 " \
                "--rename-interface-variable in 21 PARAM_21 " \
                "--rename-interface-variable in 22 PARAM_22 " \
                "--rename-interface-variable in 23 PARAM_23 " \
                "--rename-interface-variable in 24 PARAM_24 " \
                "--rename-interface-variable in 25 PARAM_25 " \
                "--rename-interface-variable in 26 PARAM_26 " \
                "--rename-interface-variable in 27 PARAM_27 " \
                "--rename-interface-variable in 28 PARAM_28 " \
                "--rename-interface-variable in 29 PARAM_29 " \
                "--rename-interface-variable in 30 PARAM_30 " \
                "--rename-interface-variable in 31 PARAM_31 " \
                "--rename-interface-variable in 32 PARAM_32 " \
                "--rename-interface-variable in 33 PARAM_33 " \
                "--rename-interface-variable in 34 PARAM_34 " \
                "--rename-interface-variable in 35 PARAM_35 " \
                "--rename-interface-variable in 36 PARAM_36 " \
                "--rename-interface-variable in 37 PARAM_37 " \
                "--rename-interface-variable in 38 PARAM_38 " \
                "--rename-interface-variable in 39 PARAM_39 " \
                "--rename-interface-variable in 40 PARAM_40 " \
                "--rename-interface-variable in 41 PARAM_41 " \
                "--rename-interface-variable in 42 PARAM_42 " \
                "--rename-interface-variable in 43 PARAM_43 " \
                "--rename-interface-variable in 44 PARAM_44 " \
                "--rename-interface-variable in 45 PARAM_45 " \
                "--rename-interface-variable in 46 PARAM_46 " \
                "--rename-interface-variable in 47 PARAM_47 " \
                "--rename-interface-variable in 48 PARAM_48 " \
                "--rename-interface-variable in 49 PARAM_49 " \
                "--rename-interface-variable in 50 PARAM_50 " \
                "--rename-interface-variable in 51 PARAM_51 " \
                "--rename-interface-variable in 52 PARAM_52 " \
                "--rename-interface-variable in 53 PARAM_53 " \
                "--rename-interface-variable in 54 PARAM_54 " \
                "--rename-interface-variable in 55 PARAM_55 " \
                "--rename-interface-variable in 56 PARAM_56 " \
                "--rename-interface-variable in 57 PARAM_57 " \
                "--rename-interface-variable in 58 PARAM_58 " \
                "--rename-interface-variable in 59 PARAM_59 " \
                "--rename-interface-variable in 60 PARAM_60 " \
                "--rename-interface-variable in 61 PARAM_61 " \
                "--rename-interface-variable in 62 PARAM_62 " \
                "--rename-interface-variable in 63 PARAM_63 " \
                "--version 330 --output \"" << fragmentShaderSrcPath << "\"";
                cmd = cmdStrm.str();
            }

            printf("%s\n", cmd.c_str());
            system(cmd.c_str());

            success = FileExists(fragmentShaderSrcPath.c_str());
            assert(success);

            DeleteFile(vertexShaderSpirvPath.c_str());
            DeleteFile(fragmentShaderSpirvPath.c_str());

            success = ReadFile(vertexShaderSrcPath, &glVertexShader);
            assert(success);

            success = ReadFile(fragmentShaderSrcPath, &glFragmentShader);
            assert(success);

            ReplaceString(glVertexShader, "\r\n", "\n");
            ReplaceString(glFragmentShader, "\r\n", "\n");

            if (vertexShader->shaderMode == GX2_SHADER_MODE_UNIFORM_REGISTERS)
            {
                const std::string& formatOldStr = "layout(std430) readonly buffer CFILE_DATA";
                const std::string& formatNewStr = "layout(std140) uniform VS_CFILE_DATA";

                ReplaceString(glVertexShader, formatOldStr, formatNewStr);
            }
            else
            {
                std::vector<GX2UniformBlock> vertexUBOs = std::vector<GX2UniformBlock>(vertexShader->uniformBlocks.get(),
                                                                                    vertexShader->uniformBlocks.get() + vertexShader->numUniformBlocks);
    
                std::sort(vertexUBOs.begin(), vertexUBOs.end(), GX2UniformBlockComp);
    
                for (u32 i = 0; i < vertexShader->numUniformBlocks; i++)
                {
                    std::ostringstream formatOldStrm;
                    formatOldStrm << "layout(std430) readonly buffer CBUFFER_DATA_" << vertexUBOs[i].location << std::endl
                                << "{" << std::endl
                                << "    vec4 values[];" << std::endl
                                << "}";
    
                    std::ostringstream formatNewStrm;
                    formatNewStrm << "layout(std140) uniform " << vertexUBOs[i].name.get() << std::endl
                                << "{" << std::endl
                                << "    vec4 values[" << ((vertexUBOs[i].size + 15) / 16) << "];" << std::endl
                                << "}";
    
                    ReplaceString(glVertexShader, formatOldStrm.str(), formatNewStrm.str());
                }
            }

            if (pixelShader->shaderMode == GX2_SHADER_MODE_UNIFORM_REGISTERS)
            {
                const std::string& formatOldStr = "layout(std430) readonly buffer CFILE_DATA";
                const std::string& formatNewStr = "layout(std140) uniform PS_CFILE_DATA";

                ReplaceString(glFragmentShader, formatOldStr, formatNewStr);
            }
            else
            {
                std::vector<GX2UniformBlock> pixelUBOs = std::vector<GX2UniformBlock>(pixelShader->uniformBlocks.get(),
                                                                                    pixelShader->uniformBlocks.get() + pixelShader->numUniformBlocks);
    
                std::sort(pixelUBOs.begin(), pixelUBOs.end(), GX2UniformBlockComp);
    
                for (u32 i = 0; i < pixelShader->numUniformBlocks; i++)
                {
                    std::ostringstream formatOldStrm;
                    formatOldStrm << "layout(std430) readonly buffer CBUFFER_DATA_" << pixelUBOs[i].location << std::endl
                                << "{" << std::endl
                                << "    vec4 values[];" << std::endl
                                << "}";
    
                    std::ostringstream formatNewStrm;
                    formatNewStrm << "layout(std140) uniform " << pixelUBOs[i].name.get() << std::endl
                                << "{" << std::endl
                                << "    vec4 values[" << ((pixelUBOs[i].size + 15) / 16) << "];" << std::endl
                                << "}";
    
                    ReplaceString(glFragmentShader, formatOldStrm.str(), formatNewStrm.str());
                }
            }

            for (u32 i = 0; i < vertexShader->numSamplers; i++)
            {
                const GX2SamplerVar& sampler = *vertexShader->samplerVars.getIndexed(i);
                if (sampler.type != GX2_SAMPLER_TYPE_2D)
                    continue;

                // Remove dummy 2D samplers definitions
                {
                    std::ostringstream formatOldStrm;
                    formatOldStrm << "uniform sampler2D SPIRV_Cross_CombinedTEXTURE_"
                                  << sampler.location
                                  << "SPIRV_Cross_DummySampler;"
                                  << std::endl;

                    ReplaceString(glVertexShader, formatOldStrm.str(), "");
                }
                // // Replace dummy 2D samplers
                // {
                //     std::ostringstream formatOldStrm;
                //     formatOldStrm << "SPIRV_Cross_CombinedTEXTURE_"
                //                   << sampler.location
                //                   << "SPIRV_Cross_DummySampler";

                //     ReplaceString(glVertexShader, formatOldStrm.str(), sampler.name.get());
                // }
                // // Replace 2D samplers
                // {
                //     std::ostringstream formatOldStrm;
                //     formatOldStrm << "SPIRV_Cross_CombinedTEXTURE_"
                //                   << sampler.location
                //                   << "SAMPLER_"
                //                   << sampler.location;

                //     ReplaceString(glVertexShader, formatOldStrm.str(), sampler.name.get());
                // }
            }

            for (u32 i = 0; i < pixelShader->numSamplers; i++)
            {
                const GX2SamplerVar& sampler = *pixelShader->samplerVars.getIndexed(i);
                if (sampler.type != GX2_SAMPLER_TYPE_2D)
                    continue;

                // Remove dummy 2D samplers definitions
                {
                    std::ostringstream formatOldStrm;
                    formatOldStrm << "uniform sampler2D SPIRV_Cross_CombinedTEXTURE_"
                                  << sampler.location
                                  << "SPIRV_Cross_DummySampler;"
                                  << std::endl;

                    ReplaceString(glFragmentShader, formatOldStrm.str(), "");
                }
                // // Replace dummy 2D samplers
                // {
                //     std::ostringstream formatOldStrm;
                //     formatOldStrm << "SPIRV_Cross_CombinedTEXTURE_"
                //                   << sampler.location
                //                   << "SPIRV_Cross_DummySampler";

                //     ReplaceString(glFragmentShader, formatOldStrm.str(), sampler.name.get());
                // }
                // // Replace 2D samplers
                // {
                //     std::ostringstream formatOldStrm;
                //     formatOldStrm << "SPIRV_Cross_CombinedTEXTURE_"
                //                   << sampler.location
                //                   << "SAMPLER_"
                //                   << sampler.location;

                //     ReplaceString(glFragmentShader, formatOldStrm.str(), sampler.name.get());
                // }
            }

            static const std::array<std::string, 5> qualifiers = {
                "noperspective centroid ",
                "noperspective ",
                "centroid ",
                "sample ",
                "flat "
            };

            for (u32 i = 0; i < 32; i++)
            {
                std::ostringstream layoutStrm;
                layoutStrm << "layout(location = " << i << ") ";
                const std::string& layoutStr = layoutStrm.str();

                std::ostringstream paramVsOldStrm;
                paramVsOldStrm << layoutStr << "out ";

                for (const std::string& qualifier : qualifiers)
                {
                    std::ostringstream paramFsStrm;
                    paramFsStrm << layoutStr << qualifier << "in ";

                    if (glFragmentShader.find(paramFsStrm.str()) != std::string::npos)
                    {
                        std::ostringstream paramVsNewStrm;
                        paramVsNewStrm << layoutStr << qualifier << "out ";

                        ReplaceString(glVertexShader, paramVsOldStrm.str(), paramVsNewStrm.str());
                        break;
                    }
                }
            }

            ReplaceString(glFragmentShader, "#version 410", "#version 330");
            ReplaceString(glFragmentShader, "#extension GL_ARB_texture_query_levels : require\n", "");

            WriteFile(vertexShaderSrcPath, glVertexShader);
            WriteFile(fragmentShaderSrcPath, glFragmentShader);
        }

        g_ShaderCache.insert(ShaderCacheMap::value_type(key, ShaderCache(glVertexShader, glFragmentShader)));
    }

    heap->Free(vertexShaderBuf);
    heap->Free(pixelShaderBuf);
    heap->Free(shaderBuf);

    *vertexShaderSrc = glVertexShader;
    *fragmentShaderSrc = glFragmentShader;
}

namespace nw { namespace eft {

TextureSampler::TextureSampler()
{
    glGenSamplers(1, &mTextureSampler);

    glSamplerParameteri(mTextureSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glSamplerParameteri(mTextureSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glSamplerParameteri(mTextureSampler, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glSamplerParameteri(mTextureSampler, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glSamplerParameteri(mTextureSampler, GL_TEXTURE_WRAP_R, GL_REPEAT);
}

TextureSampler::~TextureSampler()
{
    if (mTextureSampler != GL_NONE)
    {
        glDeleteSamplers(1, &mTextureSampler);
        mTextureSampler = GL_NONE;
    }
}

bool TextureSampler::Setup(TextureFilterMode textureFilter, TextureWrapMode wrapModeU, TextureWrapMode wrapModeV)
{
    if (textureFilter == EFT_TEXTURE_FILTER_TYPE_LINEAR)
    {
        glSamplerParameteri(mTextureSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glSamplerParameteri(mTextureSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    else
    {
        glSamplerParameteri(mTextureSampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glSamplerParameteri(mTextureSampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }

    GLenum clampX = GL_MIRRORED_REPEAT;
    GLenum clampY = GL_MIRRORED_REPEAT;
    GLenum clampZ = GL_REPEAT;

    switch (wrapModeU)
    {
    case EFT_TEXTURE_WRAP_TYPE_MIRROR:              clampX = GL_MIRRORED_REPEAT;      break;
    case EFT_TEXTURE_WRAP_TYPE_REPEAT:              clampX = GL_REPEAT;               break;
    case EFT_TEXTURE_WRAP_TYPE_CLAMP:               clampX = GL_CLAMP_TO_EDGE;        break;
    case EFT_TEXTURE_WRAP_TYPE_MIROOR_ONCE:         clampX = GL_MIRROR_CLAMP_TO_EDGE; break;
    }

    switch (wrapModeV)
    {
    case EFT_TEXTURE_WRAP_TYPE_MIRROR:              clampY = GL_MIRRORED_REPEAT;      break;
    case EFT_TEXTURE_WRAP_TYPE_REPEAT:              clampY = GL_REPEAT;               break;
    case EFT_TEXTURE_WRAP_TYPE_CLAMP:               clampY = GL_CLAMP_TO_EDGE;        break;
    case EFT_TEXTURE_WRAP_TYPE_MIROOR_ONCE:         clampY = GL_MIRROR_CLAMP_TO_EDGE; break;
    }

    glSamplerParameteri(mTextureSampler, GL_TEXTURE_WRAP_S, clampX);
    glSamplerParameteri(mTextureSampler, GL_TEXTURE_WRAP_T, clampY);
    glSamplerParameteri(mTextureSampler, GL_TEXTURE_WRAP_R, clampZ);

    return true;
}

bool TextureSampler::SetupLOD(f32 maxMip, f32 bais)
{
    glSamplerParameterf(mTextureSampler, GL_TEXTURE_MIN_LOD, 0.0f);
    glSamplerParameterf(mTextureSampler, GL_TEXTURE_MAX_LOD, maxMip);
    glSamplerParameterf(mTextureSampler, GL_TEXTURE_LOD_BIAS, bais);

    return true;
}

Rendercontext::Rendercontext()
{
    for (u32 i = 0; i < EFT_TEXTURE_SLOT_MAX; i++)
        mTextureSampler[i].Setup(EFT_TEXTURE_FILTER_TYPE_LINEAR, EFT_TEXTURE_WRAP_TYPE_REPEAT, EFT_TEXTURE_WRAP_TYPE_REPEAT);

    mDefaultTextureSampler.Setup(EFT_TEXTURE_FILTER_TYPE_LINEAR, EFT_TEXTURE_WRAP_TYPE_MIRROR, EFT_TEXTURE_WRAP_TYPE_MIRROR);

    glGenVertexArrays(1, &mVertexArrayId);
}

Rendercontext::~Rendercontext()
{
    if (mVertexArrayId != GL_NONE)
    {
        glDeleteVertexArrays(1, &mVertexArrayId);
        mVertexArrayId = GL_NONE;
    }
}

void Rendercontext::SetupCommonState()
{
    // glEnable(GL_TEXTURE_2D);

    glFrontFace(GL_CCW);
    glDisable(GL_CULL_FACE);

    glEnable(GL_BLEND);

    glBindVertexArray(mVertexArrayId);
    glBindBuffer(GL_ARRAY_BUFFER, GL_NONE);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_NONE);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
    glDisableVertexAttribArray(3);
    glDisableVertexAttribArray(4);
    glDisableVertexAttribArray(5);
    glDisableVertexAttribArray(6);
    glDisableVertexAttribArray(7);
    glDisableVertexAttribArray(8);
    glDisableVertexAttribArray(9);
    glDisableVertexAttribArray(10);
    glDisableVertexAttribArray(11);
    glDisableVertexAttribArray(12);
    glDisableVertexAttribArray(13);
    glDisableVertexAttribArray(14);
    glDisableVertexAttribArray(15);
}

void Rendercontext::SetupBlendType(BlendType blendType)
{
    switch (blendType)
    {
    case EFT_BLEND_TYPE_NORMAL:
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBlendEquation(GL_FUNC_ADD);
        break;
    case EFT_BLEND_TYPE_ADD:
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glBlendEquation(GL_FUNC_ADD);
        break;
    case EFT_BLEND_TYPE_SUB:
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
        break;
    case EFT_BLEND_TYPE_SCREEN:
        glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ONE);
        glBlendEquation(GL_FUNC_ADD);
        break;
    case EFT_BLEND_TYPE_MULT:
        glBlendFunc(GL_ZERO, GL_SRC_COLOR);
        glBlendEquation(GL_FUNC_ADD);
        break;
    }
}

void Rendercontext::SetupZBufATest(ZBufATestType zBufATestType)
{
    switch (zBufATestType)
    {
    case EFT_ZBUFF_ATEST_TYPE_NORMAL:
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDepthFunc(GL_LEQUAL);
        Shader::sAlphaTest = GL_GREATER;
        Shader::sAlphaTestRef = 0.0f;
        glEnable(GL_BLEND);
        break;
    case EFT_ZBUFF_ATEST_TYPE_ZIGNORE:
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDepthFunc(GL_LEQUAL);
        Shader::sAlphaTest = GL_GREATER;
        Shader::sAlphaTestRef = 0.0f;
        glEnable(GL_BLEND);
        break;
    case EFT_ZBUFF_ATEST_TYPE_ENTITY:
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LEQUAL);
        Shader::sAlphaTest = GL_GREATER;
        Shader::sAlphaTestRef = 0.5f;
        glDisable(GL_BLEND);
        break;
    }

    // Apply Alpha Test
    Shader* shader = Shader::sBoundShader;
    if (shader)
        shader->BindShader();
}

void Rendercontext::SetupDisplaySideType(DisplaySideType displaySideType) const
{
    switch (displaySideType)
    {
    case EFT_DISPLAYSIDETYPE_BOTH:
        glDisable(GL_CULL_FACE);
        break;
    case EFT_DISPLAYSIDETYPE_FRONT:
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        break;
    case EFT_DISPLAYSIDETYPE_BACK:
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        break;
    }
}

void Rendercontext::SetupTexture(const TextureRes* texture, TextureSlot slot, FragmentTextureLocation location)
{
    if (texture == NULL || texture->handle == GL_NONE || location.loc == EFT_INVALID_LOCATION)
        return;

    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, texture->handle);

    mTextureSampler[slot].Setup(static_cast<TextureFilterMode>(texture->filterMode),
                                static_cast<TextureWrapMode>( texture->wrapMode       & 0xF),
                                static_cast<TextureWrapMode>((texture->wrapMode >> 4) & 0xF));
    mTextureSampler[slot].SetupLOD(texture->enableMipLevel, texture->mipMapBias);

    glBindSampler(slot, mTextureSampler[slot].GetSampler());
    glUniform1i(location.loc, slot);
}

void Rendercontext::SetupTexture(const GLuint texture, TextureSlot slot, FragmentTextureLocation location)
{
    if (texture == GL_NONE || location.loc == EFT_INVALID_LOCATION)
        return;

    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, texture);

    glBindSampler(slot, mDefaultTextureSampler.GetSampler());
    glUniform1i(location.loc, slot);
}

VertexBuffer::VertexBuffer()
{
    mVertexBufferSize   = 0;
    mVertexBuffer       = NULL;
}

void* VertexBuffer::AllocateVertexBuffer(Heap* heap, u32 size, u32 element)
{
    glGenBuffers(1, &mVertexBufferId);

    mVertexBufferSize = size;
    mVertexBuffer = heap->Alloc(mVertexBufferSize);
    mVertexElement = element;
    return mVertexBuffer;
}

void VertexBuffer::Finalize(Heap* heap)
{
    if (mVertexBuffer != NULL)
    {
        heap->Free(mVertexBuffer);
        mVertexBuffer = NULL;
    }

    if (mVertexBufferId != GL_NONE)
    {
        glDeleteBuffers(1, &mVertexBufferId);
        mVertexBufferId = GL_NONE;
    }
}

void VertexBuffer::Invalidate()
{
    if (mVertexBuffer == NULL || mVertexBufferSize < sizeof(u32))
        return;

    u32* buf_32 = (u32*)mVertexBuffer;
    for (u32 i = 0; i < mVertexBufferSize; i += sizeof(u32))
    {
        *buf_32 = __builtin_bswap32(*buf_32);
        buf_32++;
    }
}

void VertexBuffer::BindBuffer(u32 index, u32 size, u32 stride)
{
    if (index == EFT_INVALID_ATTRIBUTE)
        return;

    // assert(stride == 4 * mVertexElement);

    glBindBuffer(GL_ARRAY_BUFFER, mVertexBufferId);
    glBufferData(GL_ARRAY_BUFFER, mVertexBufferSize, mVertexBuffer, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(index);
    glVertexAttribPointer(index, mVertexElement, GL_FLOAT, GL_FALSE, stride, (void*)0);
}

void VertexBuffer::BindExtBuffer(u32 index, u32 element, u32 stride, u32 offset)
{
    if (index == EFT_INVALID_ATTRIBUTE)
        return;

    glEnableVertexAttribArray(index);
    glVertexAttribPointer(index, element, GL_FLOAT, GL_FALSE, stride, (void*)offset);
}

Shader* Shader::sBoundShader    = NULL;

GLenum  Shader::sAlphaTest      = GL_ALWAYS;
f32     Shader::sAlphaTestRef   = 0.5f;

Shader::Shader()
{
    mCurrentAlphaTest           = GL_ALWAYS;
    mCurrentAlphaTestRef        = 0.5f;

    mpVertexShader              = NULL;
    mpPixelShader               = NULL;
  //mpGeometryShader            = NULL;

    mGFDFile                    = NULL;
    mProgram                    = GL_NONE;
}

void Shader::Finalize(Heap* heap)
{
    if (mGFDFile)
    {
        mGFDFile->~GFDFile();
        heap->Free(mGFDFile);

        mpVertexShader      = NULL;
        mpPixelShader       = NULL;
      //mpGeometryShader    = NULL;
    }

    if (mProgram != GL_NONE)
    {
        glDeleteProgram(mProgram);
        mProgram = GL_NONE;
        sBoundShader = NULL;
    }
}

void Shader::BindShader()
{
    if (mProgram != GL_NONE)
    {
        glUseProgram(mProgram);
        sBoundShader = this;

        if (sAlphaTest != mCurrentAlphaTest)
        {
            switch (sAlphaTest)
            {
            case GL_NEVER:
                glUniform1ui(glGetUniformLocation(mProgram, "PS_PUSH.alphaFunc"), 0); break;
            case GL_LESS:
                glUniform1ui(glGetUniformLocation(mProgram, "PS_PUSH.alphaFunc"), 1); break;
            case GL_EQUAL:
                glUniform1ui(glGetUniformLocation(mProgram, "PS_PUSH.alphaFunc"), 2); break;
            case GL_LEQUAL:
                glUniform1ui(glGetUniformLocation(mProgram, "PS_PUSH.alphaFunc"), 3); break;
            case GL_GREATER:
                glUniform1ui(glGetUniformLocation(mProgram, "PS_PUSH.alphaFunc"), 4); break;
            case GL_NOTEQUAL:
                glUniform1ui(glGetUniformLocation(mProgram, "PS_PUSH.alphaFunc"), 5); break;
            case GL_GEQUAL:
                glUniform1ui(glGetUniformLocation(mProgram, "PS_PUSH.alphaFunc"), 6); break;
            case GL_ALWAYS:
            default:
                glUniform1ui(glGetUniformLocation(mProgram, "PS_PUSH.alphaFunc"), 7); break;
            }
            mCurrentAlphaTest = sAlphaTest;
        }

        if (sAlphaTestRef != mCurrentAlphaTestRef)
        {
            glUniform1f (glGetUniformLocation(mProgram, "PS_PUSH.alphaRef"), sAlphaTestRef);
            mCurrentAlphaTestRef = sAlphaTestRef;
        }
    }
}

bool Shader::CreateShader(Heap* heap, const void* binary, u32 binarySize)
{
    mGFDFile = new (heap->Alloc(sizeof(GFDFile))) GFDFile;
    size_t loadedSize = mGFDFile->load(binary);
    assert(loadedSize == binarySize);
    //printf("Shader, binary size: %d\n", binarySize);

    mpVertexShader      = &mGFDFile->mVertexShaders[0];
    mpPixelShader       = &mGFDFile->mPixelShaders[0];
  //mpGeometryShader    = mGFDFile->mGeometryShaders.size() > 0 ? &mGFDFile->mGeometryShaders[0] : NULL;

    assert(mpVertexShader != NULL);
    assert(mpPixelShader != NULL);
    assert(mGFDFile->mGeometryShaders.size() == 0);

    std::string vertexShaderSrc;
    std::string fragmentShaderSrc;
    DecompileProgram(heap,
                     mpVertexShader,
                     mpPixelShader,
                     &vertexShaderSrc,
                     &fragmentShaderSrc);

    mProgram = CompileProgram(vertexShaderSrc.c_str(), fragmentShaderSrc.c_str());
    if (mProgram == GL_NONE)
        return false;

    glUseProgram(mProgram);

    glUniform4f(glGetUniformLocation(mProgram, "VS_PUSH.posMulAdd"), 1.0f, -1.0f, 0.0f, 0.0f);
    glUniform4f(glGetUniformLocation(mProgram, "VS_PUSH.zSpaceMul"), 0.0f,  1.0f, 1.0f, 1.0f);
    glUniform1f(glGetUniformLocation(mProgram, "VS_PUSH.pointSize"), 1.0f);

    glUniform1ui(glGetUniformLocation(mProgram, "PS_PUSH.alphaFunc"),        4);
    glUniform1f (glGetUniformLocation(mProgram, "PS_PUSH.alphaRef"),         0.0f);
    glUniform1ui(glGetUniformLocation(mProgram, "PS_PUSH.needsPremultiply"), 0);

    mCurrentAlphaTest = GL_GREATER;
    mCurrentAlphaTestRef = 0.0f;

    BindShader();

    return true;
}

u32 Shader::GetFragmentSamplerLocation(const char* name)
{
    if (!IsInitialized())
        return EFT_INVALID_SAMPLER;

    for (u32 i = 0; i < mpPixelShader->numSamplers; i++)
    {
        if (std::strcmp(mpPixelShader->samplerVars.getIndexed(i)->name.get(), name) == 0)
        {
            assert(mpPixelShader->samplerVars.getIndexed(i)->type == 1);
            u32 location = mpPixelShader->samplerVars.getIndexed(i)->location;
            std::ostringstream samplerNameStrm;
            samplerNameStrm << "SPIRV_Cross_CombinedTEXTURE_" << location << "SAMPLER_" << location;
            return glGetUniformLocation(mProgram, samplerNameStrm.str().c_str());
        }
    }

    return EFT_INVALID_SAMPLER;
}

u32 Shader::GetAttribute(const char* name, u32 index, VertexFormat fmt, u32 offset, bool instancingAttr)
{
    if (!IsInitialized())
        return EFT_INVALID_ATTRIBUTE;

    for (u32 i = 0; i < mpVertexShader->numAttribs; i++)
    {
        if (std::strcmp(mpVertexShader->attribVars.getIndexed(i)->name.get(), name) == 0)
        {
            std::ostringstream attribNameStrm;
            attribNameStrm << name << "_0_0";
            return glGetAttribLocation(mProgram, attribNameStrm.str().c_str());
        }
    }

    return EFT_INVALID_ATTRIBUTE;
}

bool UniformBlock::InitializeVertexUniformBlock(Shader* shader, const char* name, u32 bindPoint)
{
    if (!shader->IsInitialized())
    {
failure:
        mIsFailed = true;
        return false;
    }

    mBufferBindPoint = bindPoint;

    mBufferIndex = glGetUniformBlockIndex(shader->GetProgramID(), name);
    if (mBufferIndex == GL_INVALID_INDEX)
        goto failure;

    for (u32 i = 0; i < shader->GetVertexShader()->numUniformBlocks; i++)
    {
        if (std::strcmp(shader->GetVertexShader()->uniformBlocks.getIndexed(i)->name.get(), name) == 0)
        {
            mBufferSize = shader->GetVertexShader()->uniformBlocks.getIndexed(i)->size;
            goto continue_1;
        }
    }

    goto failure;

continue_1:
    glGenBuffers(1, &mBufferId);
    glBindBuffer(GL_UNIFORM_BUFFER, mBufferId);
    glBufferData(GL_UNIFORM_BUFFER, mBufferSize, NULL, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, mBufferBindPoint, mBufferId);
    glUniformBlockBinding(shader->GetProgramID(), mBufferIndex, mBufferBindPoint);

    mIsInitialized = true;

    return true;
}

bool UniformBlock::InitializePixelUniformBlock(Shader* shader, const char* name, u32 bindPoint)
{
    if (!shader->IsInitialized())
    {
failure:
        mIsFailed = true;
        return false;
    }

    mBufferBindPoint = bindPoint;

    mBufferIndex = glGetUniformBlockIndex(shader->GetProgramID(), name);
    if (mBufferIndex == GL_INVALID_INDEX)
        goto failure;

    for (u32 i = 0; i < shader->GetPixelShader()->numUniformBlocks; i++)
    {
        if (std::strcmp(shader->GetPixelShader()->uniformBlocks.getIndexed(i)->name.get(), name) == 0)
        {
            mBufferSize = shader->GetPixelShader()->uniformBlocks.getIndexed(i)->size;
            goto continue_1;
        }
    }

    goto failure;

continue_1:
    glGenBuffers(1, &mBufferId);
    glBindBuffer(GL_UNIFORM_BUFFER, mBufferId);
    glBufferData(GL_UNIFORM_BUFFER, mBufferSize, NULL, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, mBufferBindPoint, mBufferId);
    glUniformBlockBinding(shader->GetProgramID(), mBufferIndex, mBufferBindPoint);

    mIsInitialized = true;

    return true;
}

void UniformBlock::BindUniformBlock(const void* buffer)
{
    if (!mIsInitialized || mBufferSize == 0)
        return;

    glBindBuffer(GL_UNIFORM_BUFFER, mBufferId);
    glBindBufferBase(GL_UNIFORM_BUFFER, mBufferBindPoint, mBufferId);
    glBufferData(GL_UNIFORM_BUFFER, mBufferSize, buffer, GL_DYNAMIC_DRAW);
}

} } // namespace nw::eft

#endif // EFT_WIN
