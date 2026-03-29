#include "rt/scene/scene_loader.h"
#include "tmpl8/template.h"

namespace rt::scene {

    // ----------------------------------------------------------------------------
    // ProceduralSceneLoader - Perlin noise terrain
    // ----------------------------------------------------------------------------
    void ProceduralSceneLoader::load( Scene& scene )
    {
        for (int z = 0; z < 128; z++)
        {
            const float fz = static_cast<float>(z) / WORLDSIZE;
            for (int y = 0; y < WORLDSIZE; y++)
            {
                float fx = 0, fy = static_cast<float>(y) / WORLDSIZE;
                for (int x = 0; x < WORLDSIZE; x++, fx += 1.0f / WORLDSIZE)
                {
                    const float n = noise3D( fx, fy, fz );
                    scene.set( x, y, z, n > 0.09f ? 0x020101 * y : 0 );
                }
            }
        }
    }

    // ----------------------------------------------------------------------------
    // FileSceneLoader - compressed .bin files (zlib)
    // ----------------------------------------------------------------------------
    FileSceneLoader::FileSceneLoader( const std::string& path )
        : m_filePath( path )
    { }

    void FileSceneLoader::load( Scene& scene )
    {
        gzFile f = gzopen(m_filePath.c_str(), "rb");
        int3 size{};
        gzread(f, &size, sizeof(int3));

        uint* tmp = new uint[size.x * size.y * size.z];
        gzread(f, tmp, size.x * size.y * size.z * 4);
        gzclose(f);

        for (int z = 0; z < size.z; z++)
            for (int y = 0; y < size.y; y++)
                for (int x = 0; x < size.x; x++)
                    scene.set(x, y, z, tmp[x + y * size.x + z * size.x * size.y]);

        delete[] tmp;
    }

}  // Tmpl8