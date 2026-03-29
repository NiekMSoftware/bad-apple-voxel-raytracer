#pragma once

#include "scene.h"
#include <string>

namespace rt::scene {

    // ----------------------------------------------------------------------------
    // Abstract base class for scene loaders
    // ----------------------------------------------------------------------------
    class SceneLoader
    {
    public:
        virtual ~SceneLoader() = default;
        virtual void load( Scene& scene ) = 0;
    };

    // ----------------------------------------------------------------------------
    // Loads a scene using procedural Perlin noise generation
    // ----------------------------------------------------------------------------
    class ProceduralSceneLoader : public SceneLoader
    {
    public:
        void load( Scene& scene ) override;
    };

    // ----------------------------------------------------------------------------
    // Loads a compressed .bin scene file (zlib format)
    // ----------------------------------------------------------------------------
    class FileSceneLoader : public SceneLoader
    {
    public:
        explicit FileSceneLoader( const std::string& path );
        void load( Scene& scene ) override;
    private:
        std::string m_filePath;
    };

}