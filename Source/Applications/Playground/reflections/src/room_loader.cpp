#include "room_loader.h"
#include "rt/rendering/bit_packer.h"

namespace Tmpl8 {

    namespace MatID {
        constexpr uint DIFFUSE          = 0;
        constexpr uint MIRROR           = 1;
        constexpr uint GLASS            = 2;
        constexpr uint ROUGH_PLASTIC    = 3;
        constexpr uint GLOSSY_PLASTIC   = 4;
        constexpr uint POLISHED_METAL   = 5;
        constexpr uint BRUSHED_METAL    = 6;
        constexpr uint RUSTED_METAL     = 7;
        constexpr uint CERAMIC          = 8;
        constexpr uint RUBBER           = 9;
        constexpr uint WATER            = 10;
        constexpr uint DIAMOND          = 11;
        constexpr uint GOLD             = 12;
        constexpr uint COPPER           = 13;
    }

    void RoomLoader::Load(Scene &scene) {
        // Wall thickness and margin, proportional to world size.
        // At WORLDSIZE=128 the original used 2-voxel walls and a 2-voxel gap at the top,
        // so we scale: thickness = WORLDSIZE/64 gives 2 at 128, 8 at 512.
        constexpr int wallThick = WORLDSIZE / 64;
        constexpr int maxCoord  = WORLDSIZE - wallThick;  // inner boundary

        // Cube dimensions, scaled proportionally.
        // Original at 128: cubes spanned y[30..50], z[50..70], various x ranges.
        // We express them as fractions of WORLDSIZE.
        constexpr int cubeYMin = WORLDSIZE * 30 / 128;
        constexpr int cubeYMax = WORLDSIZE * 50 / 128;
        constexpr int cubeZMin = WORLDSIZE * 50 / 128;
        constexpr int cubeZMax = WORLDSIZE * 70 / 128;
        constexpr int cubeXStart = WORLDSIZE * 20 / 128;

        // Individual cube x-ranges (original at 128: 20-40, 55-75, 90-110)
        constexpr int plasticXMax = WORLDSIZE * 40 / 128;
        constexpr int glassXMin   = WORLDSIZE * 55 / 128;
        constexpr int glassXMax   = WORLDSIZE * 75 / 128;
        constexpr int metalXMin   = WORLDSIZE * 90 / 128;
        constexpr int metalXMax   = WORLDSIZE * 110 / 128;

        for (int z = 0; z < WORLDSIZE; z++) {
            for (int y = 0; y < WORLDSIZE; y++) {
                for (int x = 0; x < WORLDSIZE; x++) {

                    // Walls, floor, and ceiling
                    if (x < wallThick || x >= maxCoord ||
                        z >= maxCoord  ||
                        y < wallThick  || y >= maxCoord) {

                        uint voxel = 0;

                        if (y < wallThick) {
                            // Floor - ceramic tiles
                            voxel = PackMaterial(MatID::CERAMIC);
                        }
                        else if (y >= maxCoord) {
                            // Ceiling - matte diffuse
                            voxel = PackMaterial(MatID::DIFFUSE);
                        }
                        else {
                            // Walls - diffuse
                            voxel = PackMaterial(MatID::DIFFUSE);
                        }

                        scene.Set(x, y, z, voxel);
                    }

                    // Cubes in the room
                    else if (y > cubeYMin && y < cubeYMax &&
                             z > cubeZMin && z < cubeZMax &&
                             x > cubeXStart) {

                        // Rough plastic cube
                        if (x < plasticXMax) {
                            const uint voxel = PackMaterial(MatID::ROUGH_PLASTIC);
                            scene.Set(x, y, z, voxel);
                        }
                        // Glass cube
                        else if (x > glassXMin && x < glassXMax) {
                            const uint voxel = PackMaterial(MatID::GLASS);
                            scene.Set(x, y, z, voxel);
                        }
                        // Polished metal cube
                        else if (x > metalXMin && x < metalXMax) {
                            const uint voxel = PackMaterial(MatID::POLISHED_METAL);
                            scene.Set(x, y, z, voxel);
                        }
                    }
                }
            }
        }
    }

}  // Tmpl8
