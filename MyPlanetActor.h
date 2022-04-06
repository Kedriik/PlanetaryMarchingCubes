// Fill out your copyright notice in the Description page of Project Settings.
#pragma once
#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include <vector>
#include <random>
#include "CoreMinimal.h"
#include "Actors/PlanetActor.h"
#include "FoliageInstancedStaticMeshComponent.h"
#include <mutex>
#include <functional>
#include <math.h>
#include "MyPlanetActor.generated.h"
static int closestMultiple(int n, int x)
{
    bool flag = false;
    if (n < 0) {
        flag = true;
        n *= -1;
    }
    if (x > n)
        return x;

    n = n + x / 2;
    n = n - (n % x);

    return flag ? n * -1 : n;
};
static float  getRandom(float min, float max, float seed)
{
    std::default_random_engine e;
    e.seed(seed);
    std::uniform_real_distribution<> dis(min, max); // rage 0 - 1
    return dis(e);
};
static int vertexExists(TArray<FVector> vertices, FVector vertex, float epsilon) {
    for (int i = 0; i < vertices.Num(); i++) {
        if (FVector::Distance(vertices[i], vertex) <= epsilon) {
            return i;
        }
    }
    return -1;
}
static float m_hash(FVector seed) {
    uint64_t result = uint16_t(seed.X);
    result = (result << 16) + uint16_t(seed.Y);
    result = (result << 16) + uint16_t(seed.Z);
    double i;
    return modf(sinf(seed.X) * 43758.54f, &i) + modf(sinf(seed.Y) * 97859.954f, &i) + modf(sinf(seed.Z) * 25632.054f, &i);;
}
static float getRandom(float min, float max, FVector seed) {
    return getRandom(min, max, m_hash(seed));
}
static std::vector<FVector> randomPoints(int num, FVector seed, float dist) {
    std::vector<FVector> locs;
    for (int i = 0; i < num; i++) {
        FVector loc = FVector(getRandom(-1, 1, seed),  getRandom(-1, 1, 2.0 * seed),  getRandom(-1, 1, 4.0 * seed));
        loc.Normalize();
        locs.push_back(seed+loc*dist);
    }
    return locs;
}
UCLASS()
class CUSTOMGRAVITYPROJECT_API AMyPlanetActor : public APlanetActor
{
	GENERATED_BODY()
	
public:
    // Sets default values for this actor's properties
    AMyPlanetActor();
    UPROPERTY(EditAnywhere, Category = "Materials")
        UMaterialInterface* vertexColorMaterial;

    UPROPERTY(EditAnywhere, Category = "Materials")
        UMaterialInterface* planetMaterial;

    UPROPERTY(EditAnywhere, Category = "PlanetProperties")
        UTexture2D* PlanetTopography;

    UPROPERTY(EditAnywhere, Category = "PlanetProperties")
        UTexture2D* PlanetColor;

    UPROPERTY(EditAnywhere, Category = "PlanetProperties")
        float Radius = 338950;

    UPROPERTY(EditAnywhere, Category = "GeometryProperties")
        float GenerateGeometriesDist = 500;

    UPROPERTY(EditAnywhere, Category = "GeometryProperties")
        float IndexGridcellsDist = 1000;

    UPROPERTY(EditAnywhere, Category = "GeometryProperties")
        float PlaceNodeDist = 2000;

    UPROPERTY(EditAnywhere, Category = "GeometryProperties")
        float RenderDistance = 10000;

    UPROPERTY(EditAnywhere, Category = "GeometryProperties")
        float GeometryNodeSize = 1000;

    UPROPERTY(EditAnywhere, Category = "GeometryProperties")
        float NodeGridcellSize = 100;

    UPROPERTY(EditAnywhere, Category = "GeometryProperties")
        float Isolevel = 0.0;

    UPROPERTY(EditAnywhere, Category = "GeometryProperties")
        float HeightMultiplier = 1;

    UPROPERTY(EditAnywhere, Category = "GeometryProperties")
        float FirstLoadGenerationMultiplier = 1.0;

    UPROPERTY(EditAnywhere, Category = "GeometryProperties")
        bool DrawMarchingCubes = true;

    UPROPERTY(EditAnywhere, Category = "GeometryProperties")
        bool DrawLowPoly = true;

    UPROPERTY(EditAnywhere, Category = "GeometryProperties")
        float sectorCount = 100;

    UPROPERTY(EditAnywhere, Category = "GeometryProperties")
        float stackCount = 100;

    UPROPERTY(EditAnywhere, Category = "Debug")
        bool DrawDebugBoxes = false;

    UPROPERTY(EditAnywhere, Instanced, Category = "Flora")
        TArray<UFoliageInstancedStaticMeshComponent*> Trees;

protected:
    // Called when the game starts or when spawned
    uint8* raw = NULL;
    bool isGenerationReady = true;
    bool firstLookup = true;
    bool firstColorLookup = true;
    bool doManagementLoop = true;
    virtual void BeginPlay() override;
    virtual void BeginDestroy() override;
    virtual void PostActorCreated() override;
    virtual void PostLoad() override;
    std::mutex managementMutex;
    FVector currentPawnPos;
    FVector lastCollisionMeshUpdate;
    void launchNodeManagementLoop();
    const FColor* FormatedTopographyImageData;
    FColor* FormatedTopographyImageData_copy;
    const FColor* PlanetColorImageData;
    FColor getTopographyAt(FVector2D uv) {
        if (firstLookup) {
            TextureCompressionSettings OldCompressionSettings = PlanetTopography->CompressionSettings;
            TextureMipGenSettings OldMipGenSettings = PlanetTopography->MipGenSettings;
            bool OldSRGB = PlanetTopography->SRGB;

            PlanetTopography->CompressionSettings = TextureCompressionSettings::TC_VectorDisplacementmap;
            PlanetTopography->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
            PlanetTopography->SRGB = false;
            PlanetTopography->UpdateResource();
            FormatedTopographyImageData = static_cast<const FColor*>(PlanetTopography->PlatformData->Mips[0].BulkData.LockReadOnly());
            FormatedTopographyImageData_copy = new FColor[PlanetTopography->GetSizeX() * PlanetTopography->GetSizeY()];
            int test = sizeof(FColor);
            int x = PlanetTopography->GetSizeX();
            int y = PlanetTopography->GetSizeY();
            std::memcpy(FormatedTopographyImageData_copy, FormatedTopographyImageData, x*y*sizeof(FColor));
            PlanetTopography->PlatformData->Mips[0].BulkData.Unlock();
            firstLookup = false;
        }
        int X = uv.X * (PlanetTopography->GetSizeX());
        int Y = uv.Y * (PlanetTopography->GetSizeY());

        FColor PixelColor = FormatedTopographyImageData_copy[Y * (PlanetTopography->GetSizeX()) + X];
        
        return PixelColor;
    }
    FColor getColorAt(FVector2D uv) {
        if (firstColorLookup) {
            TextureCompressionSettings OldCompressionSettings = PlanetColor->CompressionSettings;
            TextureMipGenSettings OldMipGenSettings = PlanetColor->MipGenSettings;
            bool OldSRGB = PlanetColor->SRGB;

            PlanetColor->CompressionSettings = TextureCompressionSettings::TC_VectorDisplacementmap;
            PlanetColor->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
            PlanetColor->SRGB = false;
            PlanetColor->UpdateResource();
            PlanetColorImageData = static_cast<const FColor*>(PlanetColor->PlatformData->Mips[0].BulkData.LockReadOnly());
            PlanetColor->PlatformData->Mips[0].BulkData.Unlock();
            firstColorLookup = false;
        }
        int X = uv.X * (PlanetColor->GetSizeX());
        int Y = uv.Y * (PlanetColor->GetSizeY());
        FColor PixelColor = PlanetColorImageData[Y * (PlanetColor->GetSizeX()) + X];
        
        return PixelColor;
    }
    void generateSphere();
    void placeGeometryNode(FVector location);
    void placeAndGenerateNode(FVector location);

    UProceduralMeshComponent* PlanetMesh;
    //Paul Bourke
    typedef struct {
        FVector p[3];
    } TRIANGLE;
    std::vector<TRIANGLE> triangles;
    typedef struct {
        std::vector<FVector> p;
        std::vector<double> val;
    } GRIDCELL;
    struct Cube {
        /*
           Given a grid cell and an isolevel, calculate the triangular
           facets required to represent the isosurface through the cell.
           Return the number of triangular facets, the array "triangles"
           will be loaded up with the vertices at most 5 triangular facets.
            0 will be returned if the grid cell is either totally above
           of totally below the isolevel.
        */
        static int Polygonise(GRIDCELL grid, double isolevel, TRIANGLE* triangles)
        {
            int i, ntriang;
            int cubeindex;
            FVector vertlist[12];

            int edgeTable[256] =
            {
            0x0  , 0x109, 0x203, 0x30a, 0x406, 0x50f, 0x605, 0x70c,
            0x80c, 0x905, 0xa0f, 0xb06, 0xc0a, 0xd03, 0xe09, 0xf00,
            0x190, 0x99 , 0x393, 0x29a, 0x596, 0x49f, 0x795, 0x69c,
            0x99c, 0x895, 0xb9f, 0xa96, 0xd9a, 0xc93, 0xf99, 0xe90,
            0x230, 0x339, 0x33 , 0x13a, 0x636, 0x73f, 0x435, 0x53c,
            0xa3c, 0xb35, 0x83f, 0x936, 0xe3a, 0xf33, 0xc39, 0xd30,
            0x3a0, 0x2a9, 0x1a3, 0xaa , 0x7a6, 0x6af, 0x5a5, 0x4ac,
            0xbac, 0xaa5, 0x9af, 0x8a6, 0xfaa, 0xea3, 0xda9, 0xca0,
            0x460, 0x569, 0x663, 0x76a, 0x66 , 0x16f, 0x265, 0x36c,
            0xc6c, 0xd65, 0xe6f, 0xf66, 0x86a, 0x963, 0xa69, 0xb60,
            0x5f0, 0x4f9, 0x7f3, 0x6fa, 0x1f6, 0xff , 0x3f5, 0x2fc,
            0xdfc, 0xcf5, 0xfff, 0xef6, 0x9fa, 0x8f3, 0xbf9, 0xaf0,
            0x650, 0x759, 0x453, 0x55a, 0x256, 0x35f, 0x55 , 0x15c,
            0xe5c, 0xf55, 0xc5f, 0xd56, 0xa5a, 0xb53, 0x859, 0x950,
            0x7c0, 0x6c9, 0x5c3, 0x4ca, 0x3c6, 0x2cf, 0x1c5, 0xcc ,
            0xfcc, 0xec5, 0xdcf, 0xcc6, 0xbca, 0xac3, 0x9c9, 0x8c0,
            0x8c0, 0x9c9, 0xac3, 0xbca, 0xcc6, 0xdcf, 0xec5, 0xfcc,
            0xcc , 0x1c5, 0x2cf, 0x3c6, 0x4ca, 0x5c3, 0x6c9, 0x7c0,
            0x950, 0x859, 0xb53, 0xa5a, 0xd56, 0xc5f, 0xf55, 0xe5c,
            0x15c, 0x55 , 0x35f, 0x256, 0x55a, 0x453, 0x759, 0x650,
            0xaf0, 0xbf9, 0x8f3, 0x9fa, 0xef6, 0xfff, 0xcf5, 0xdfc,
            0x2fc, 0x3f5, 0xff , 0x1f6, 0x6fa, 0x7f3, 0x4f9, 0x5f0,
            0xb60, 0xa69, 0x963, 0x86a, 0xf66, 0xe6f, 0xd65, 0xc6c,
            0x36c, 0x265, 0x16f, 0x66 , 0x76a, 0x663, 0x569, 0x460,
            0xca0, 0xda9, 0xea3, 0xfaa, 0x8a6, 0x9af, 0xaa5, 0xbac,
            0x4ac, 0x5a5, 0x6af, 0x7a6, 0xaa , 0x1a3, 0x2a9, 0x3a0,
            0xd30, 0xc39, 0xf33, 0xe3a, 0x936, 0x83f, 0xb35, 0xa3c,
            0x53c, 0x435, 0x73f, 0x636, 0x13a, 0x33 , 0x339, 0x230,
            0xe90, 0xf99, 0xc93, 0xd9a, 0xa96, 0xb9f, 0x895, 0x99c,
            0x69c, 0x795, 0x49f, 0x596, 0x29a, 0x393, 0x99 , 0x190,
            0xf00, 0xe09, 0xd03, 0xc0a, 0xb06, 0xa0f, 0x905, 0x80c,
            0x70c, 0x605, 0x50f, 0x406, 0x30a, 0x203, 0x109, 0x0 };
            int triTable[256][16] =
            { {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {0, 1, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {1, 8, 3, 9, 8, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {0, 8, 3, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {9, 2, 10, 0, 2, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {2, 8, 3, 2, 10, 8, 10, 9, 8, -1, -1, -1, -1, -1, -1, -1},
            {3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {0, 11, 2, 8, 11, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {1, 9, 0, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {1, 11, 2, 1, 9, 11, 9, 8, 11, -1, -1, -1, -1, -1, -1, -1},
            {3, 10, 1, 11, 10, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {0, 10, 1, 0, 8, 10, 8, 11, 10, -1, -1, -1, -1, -1, -1, -1},
            {3, 9, 0, 3, 11, 9, 11, 10, 9, -1, -1, -1, -1, -1, -1, -1},
            {9, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {4, 3, 0, 7, 3, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {0, 1, 9, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {4, 1, 9, 4, 7, 1, 7, 3, 1, -1, -1, -1, -1, -1, -1, -1},
            {1, 2, 10, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {3, 4, 7, 3, 0, 4, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1},
            {9, 2, 10, 9, 0, 2, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
            {2, 10, 9, 2, 9, 7, 2, 7, 3, 7, 9, 4, -1, -1, -1, -1},
            {8, 4, 7, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {11, 4, 7, 11, 2, 4, 2, 0, 4, -1, -1, -1, -1, -1, -1, -1},
            {9, 0, 1, 8, 4, 7, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
            {4, 7, 11, 9, 4, 11, 9, 11, 2, 9, 2, 1, -1, -1, -1, -1},
            {3, 10, 1, 3, 11, 10, 7, 8, 4, -1, -1, -1, -1, -1, -1, -1},
            {1, 11, 10, 1, 4, 11, 1, 0, 4, 7, 11, 4, -1, -1, -1, -1},
            {4, 7, 8, 9, 0, 11, 9, 11, 10, 11, 0, 3, -1, -1, -1, -1},
            {4, 7, 11, 4, 11, 9, 9, 11, 10, -1, -1, -1, -1, -1, -1, -1},
            {9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {9, 5, 4, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {0, 5, 4, 1, 5, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {8, 5, 4, 8, 3, 5, 3, 1, 5, -1, -1, -1, -1, -1, -1, -1},
            {1, 2, 10, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {3, 0, 8, 1, 2, 10, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
            {5, 2, 10, 5, 4, 2, 4, 0, 2, -1, -1, -1, -1, -1, -1, -1},
            {2, 10, 5, 3, 2, 5, 3, 5, 4, 3, 4, 8, -1, -1, -1, -1},
            {9, 5, 4, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {0, 11, 2, 0, 8, 11, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
            {0, 5, 4, 0, 1, 5, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
            {2, 1, 5, 2, 5, 8, 2, 8, 11, 4, 8, 5, -1, -1, -1, -1},
            {10, 3, 11, 10, 1, 3, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1},
            {4, 9, 5, 0, 8, 1, 8, 10, 1, 8, 11, 10, -1, -1, -1, -1},
            {5, 4, 0, 5, 0, 11, 5, 11, 10, 11, 0, 3, -1, -1, -1, -1},
            {5, 4, 8, 5, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1},
            {9, 7, 8, 5, 7, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {9, 3, 0, 9, 5, 3, 5, 7, 3, -1, -1, -1, -1, -1, -1, -1},
            {0, 7, 8, 0, 1, 7, 1, 5, 7, -1, -1, -1, -1, -1, -1, -1},
            {1, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {9, 7, 8, 9, 5, 7, 10, 1, 2, -1, -1, -1, -1, -1, -1, -1},
            {10, 1, 2, 9, 5, 0, 5, 3, 0, 5, 7, 3, -1, -1, -1, -1},
            {8, 0, 2, 8, 2, 5, 8, 5, 7, 10, 5, 2, -1, -1, -1, -1},
            {2, 10, 5, 2, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1},
            {7, 9, 5, 7, 8, 9, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1},
            {9, 5, 7, 9, 7, 2, 9, 2, 0, 2, 7, 11, -1, -1, -1, -1},
            {2, 3, 11, 0, 1, 8, 1, 7, 8, 1, 5, 7, -1, -1, -1, -1},
            {11, 2, 1, 11, 1, 7, 7, 1, 5, -1, -1, -1, -1, -1, -1, -1},
            {9, 5, 8, 8, 5, 7, 10, 1, 3, 10, 3, 11, -1, -1, -1, -1},
            {5, 7, 0, 5, 0, 9, 7, 11, 0, 1, 0, 10, 11, 10, 0, -1},
            {11, 10, 0, 11, 0, 3, 10, 5, 0, 8, 0, 7, 5, 7, 0, -1},
            {11, 10, 5, 7, 11, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {0, 8, 3, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {9, 0, 1, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {1, 8, 3, 1, 9, 8, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
            {1, 6, 5, 2, 6, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {1, 6, 5, 1, 2, 6, 3, 0, 8, -1, -1, -1, -1, -1, -1, -1},
            {9, 6, 5, 9, 0, 6, 0, 2, 6, -1, -1, -1, -1, -1, -1, -1},
            {5, 9, 8, 5, 8, 2, 5, 2, 6, 3, 2, 8, -1, -1, -1, -1},
            {2, 3, 11, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {11, 0, 8, 11, 2, 0, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
            {0, 1, 9, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
            {5, 10, 6, 1, 9, 2, 9, 11, 2, 9, 8, 11, -1, -1, -1, -1},
            {6, 3, 11, 6, 5, 3, 5, 1, 3, -1, -1, -1, -1, -1, -1, -1},
            {0, 8, 11, 0, 11, 5, 0, 5, 1, 5, 11, 6, -1, -1, -1, -1},
            {3, 11, 6, 0, 3, 6, 0, 6, 5, 0, 5, 9, -1, -1, -1, -1},
            {6, 5, 9, 6, 9, 11, 11, 9, 8, -1, -1, -1, -1, -1, -1, -1},
            {5, 10, 6, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {4, 3, 0, 4, 7, 3, 6, 5, 10, -1, -1, -1, -1, -1, -1, -1},
            {1, 9, 0, 5, 10, 6, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
            {10, 6, 5, 1, 9, 7, 1, 7, 3, 7, 9, 4, -1, -1, -1, -1},
            {6, 1, 2, 6, 5, 1, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1},
            {1, 2, 5, 5, 2, 6, 3, 0, 4, 3, 4, 7, -1, -1, -1, -1},
            {8, 4, 7, 9, 0, 5, 0, 6, 5, 0, 2, 6, -1, -1, -1, -1},
            {7, 3, 9, 7, 9, 4, 3, 2, 9, 5, 9, 6, 2, 6, 9, -1},
            {3, 11, 2, 7, 8, 4, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
            {5, 10, 6, 4, 7, 2, 4, 2, 0, 2, 7, 11, -1, -1, -1, -1},
            {0, 1, 9, 4, 7, 8, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1},
            {9, 2, 1, 9, 11, 2, 9, 4, 11, 7, 11, 4, 5, 10, 6, -1},
            {8, 4, 7, 3, 11, 5, 3, 5, 1, 5, 11, 6, -1, -1, -1, -1},
            {5, 1, 11, 5, 11, 6, 1, 0, 11, 7, 11, 4, 0, 4, 11, -1},
            {0, 5, 9, 0, 6, 5, 0, 3, 6, 11, 6, 3, 8, 4, 7, -1},
            {6, 5, 9, 6, 9, 11, 4, 7, 9, 7, 11, 9, -1, -1, -1, -1},
            {10, 4, 9, 6, 4, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {4, 10, 6, 4, 9, 10, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1},
            {10, 0, 1, 10, 6, 0, 6, 4, 0, -1, -1, -1, -1, -1, -1, -1},
            {8, 3, 1, 8, 1, 6, 8, 6, 4, 6, 1, 10, -1, -1, -1, -1},
            {1, 4, 9, 1, 2, 4, 2, 6, 4, -1, -1, -1, -1, -1, -1, -1},
            {3, 0, 8, 1, 2, 9, 2, 4, 9, 2, 6, 4, -1, -1, -1, -1},
            {0, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {8, 3, 2, 8, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1},
            {10, 4, 9, 10, 6, 4, 11, 2, 3, -1, -1, -1, -1, -1, -1, -1},
            {0, 8, 2, 2, 8, 11, 4, 9, 10, 4, 10, 6, -1, -1, -1, -1},
            {3, 11, 2, 0, 1, 6, 0, 6, 4, 6, 1, 10, -1, -1, -1, -1},
            {6, 4, 1, 6, 1, 10, 4, 8, 1, 2, 1, 11, 8, 11, 1, -1},
            {9, 6, 4, 9, 3, 6, 9, 1, 3, 11, 6, 3, -1, -1, -1, -1},
            {8, 11, 1, 8, 1, 0, 11, 6, 1, 9, 1, 4, 6, 4, 1, -1},
            {3, 11, 6, 3, 6, 0, 0, 6, 4, -1, -1, -1, -1, -1, -1, -1},
            {6, 4, 8, 11, 6, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {7, 10, 6, 7, 8, 10, 8, 9, 10, -1, -1, -1, -1, -1, -1, -1},
            {0, 7, 3, 0, 10, 7, 0, 9, 10, 6, 7, 10, -1, -1, -1, -1},
            {10, 6, 7, 1, 10, 7, 1, 7, 8, 1, 8, 0, -1, -1, -1, -1},
            {10, 6, 7, 10, 7, 1, 1, 7, 3, -1, -1, -1, -1, -1, -1, -1},
            {1, 2, 6, 1, 6, 8, 1, 8, 9, 8, 6, 7, -1, -1, -1, -1},
            {2, 6, 9, 2, 9, 1, 6, 7, 9, 0, 9, 3, 7, 3, 9, -1},
            {7, 8, 0, 7, 0, 6, 6, 0, 2, -1, -1, -1, -1, -1, -1, -1},
            {7, 3, 2, 6, 7, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {2, 3, 11, 10, 6, 8, 10, 8, 9, 8, 6, 7, -1, -1, -1, -1},
            {2, 0, 7, 2, 7, 11, 0, 9, 7, 6, 7, 10, 9, 10, 7, -1},
            {1, 8, 0, 1, 7, 8, 1, 10, 7, 6, 7, 10, 2, 3, 11, -1},
            {11, 2, 1, 11, 1, 7, 10, 6, 1, 6, 7, 1, -1, -1, -1, -1},
            {8, 9, 6, 8, 6, 7, 9, 1, 6, 11, 6, 3, 1, 3, 6, -1},
            {0, 9, 1, 11, 6, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {7, 8, 0, 7, 0, 6, 3, 11, 0, 11, 6, 0, -1, -1, -1, -1},
            {7, 11, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {3, 0, 8, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {0, 1, 9, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {8, 1, 9, 8, 3, 1, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
            {10, 1, 2, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {1, 2, 10, 3, 0, 8, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
            {2, 9, 0, 2, 10, 9, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
            {6, 11, 7, 2, 10, 3, 10, 8, 3, 10, 9, 8, -1, -1, -1, -1},
            {7, 2, 3, 6, 2, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {7, 0, 8, 7, 6, 0, 6, 2, 0, -1, -1, -1, -1, -1, -1, -1},
            {2, 7, 6, 2, 3, 7, 0, 1, 9, -1, -1, -1, -1, -1, -1, -1},
            {1, 6, 2, 1, 8, 6, 1, 9, 8, 8, 7, 6, -1, -1, -1, -1},
            {10, 7, 6, 10, 1, 7, 1, 3, 7, -1, -1, -1, -1, -1, -1, -1},
            {10, 7, 6, 1, 7, 10, 1, 8, 7, 1, 0, 8, -1, -1, -1, -1},
            {0, 3, 7, 0, 7, 10, 0, 10, 9, 6, 10, 7, -1, -1, -1, -1},
            {7, 6, 10, 7, 10, 8, 8, 10, 9, -1, -1, -1, -1, -1, -1, -1},
            {6, 8, 4, 11, 8, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {3, 6, 11, 3, 0, 6, 0, 4, 6, -1, -1, -1, -1, -1, -1, -1},
            {8, 6, 11, 8, 4, 6, 9, 0, 1, -1, -1, -1, -1, -1, -1, -1},
            {9, 4, 6, 9, 6, 3, 9, 3, 1, 11, 3, 6, -1, -1, -1, -1},
            {6, 8, 4, 6, 11, 8, 2, 10, 1, -1, -1, -1, -1, -1, -1, -1},
            {1, 2, 10, 3, 0, 11, 0, 6, 11, 0, 4, 6, -1, -1, -1, -1},
            {4, 11, 8, 4, 6, 11, 0, 2, 9, 2, 10, 9, -1, -1, -1, -1},
            {10, 9, 3, 10, 3, 2, 9, 4, 3, 11, 3, 6, 4, 6, 3, -1},
            {8, 2, 3, 8, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1},
            {0, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {1, 9, 0, 2, 3, 4, 2, 4, 6, 4, 3, 8, -1, -1, -1, -1},
            {1, 9, 4, 1, 4, 2, 2, 4, 6, -1, -1, -1, -1, -1, -1, -1},
            {8, 1, 3, 8, 6, 1, 8, 4, 6, 6, 10, 1, -1, -1, -1, -1},
            {10, 1, 0, 10, 0, 6, 6, 0, 4, -1, -1, -1, -1, -1, -1, -1},
            {4, 6, 3, 4, 3, 8, 6, 10, 3, 0, 3, 9, 10, 9, 3, -1},
            {10, 9, 4, 6, 10, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {4, 9, 5, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {0, 8, 3, 4, 9, 5, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
            {5, 0, 1, 5, 4, 0, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
            {11, 7, 6, 8, 3, 4, 3, 5, 4, 3, 1, 5, -1, -1, -1, -1},
            {9, 5, 4, 10, 1, 2, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
            {6, 11, 7, 1, 2, 10, 0, 8, 3, 4, 9, 5, -1, -1, -1, -1},
            {7, 6, 11, 5, 4, 10, 4, 2, 10, 4, 0, 2, -1, -1, -1, -1},
            {3, 4, 8, 3, 5, 4, 3, 2, 5, 10, 5, 2, 11, 7, 6, -1},
            {7, 2, 3, 7, 6, 2, 5, 4, 9, -1, -1, -1, -1, -1, -1, -1},
            {9, 5, 4, 0, 8, 6, 0, 6, 2, 6, 8, 7, -1, -1, -1, -1},
            {3, 6, 2, 3, 7, 6, 1, 5, 0, 5, 4, 0, -1, -1, -1, -1},
            {6, 2, 8, 6, 8, 7, 2, 1, 8, 4, 8, 5, 1, 5, 8, -1},
            {9, 5, 4, 10, 1, 6, 1, 7, 6, 1, 3, 7, -1, -1, -1, -1},
            {1, 6, 10, 1, 7, 6, 1, 0, 7, 8, 7, 0, 9, 5, 4, -1},
            {4, 0, 10, 4, 10, 5, 0, 3, 10, 6, 10, 7, 3, 7, 10, -1},
            {7, 6, 10, 7, 10, 8, 5, 4, 10, 4, 8, 10, -1, -1, -1, -1},
            {6, 9, 5, 6, 11, 9, 11, 8, 9, -1, -1, -1, -1, -1, -1, -1},
            {3, 6, 11, 0, 6, 3, 0, 5, 6, 0, 9, 5, -1, -1, -1, -1},
            {0, 11, 8, 0, 5, 11, 0, 1, 5, 5, 6, 11, -1, -1, -1, -1},
            {6, 11, 3, 6, 3, 5, 5, 3, 1, -1, -1, -1, -1, -1, -1, -1},
            {1, 2, 10, 9, 5, 11, 9, 11, 8, 11, 5, 6, -1, -1, -1, -1},
            {0, 11, 3, 0, 6, 11, 0, 9, 6, 5, 6, 9, 1, 2, 10, -1},
            {11, 8, 5, 11, 5, 6, 8, 0, 5, 10, 5, 2, 0, 2, 5, -1},
            {6, 11, 3, 6, 3, 5, 2, 10, 3, 10, 5, 3, -1, -1, -1, -1},
            {5, 8, 9, 5, 2, 8, 5, 6, 2, 3, 8, 2, -1, -1, -1, -1},
            {9, 5, 6, 9, 6, 0, 0, 6, 2, -1, -1, -1, -1, -1, -1, -1},
            {1, 5, 8, 1, 8, 0, 5, 6, 8, 3, 8, 2, 6, 2, 8, -1},
            {1, 5, 6, 2, 1, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {1, 3, 6, 1, 6, 10, 3, 8, 6, 5, 6, 9, 8, 9, 6, -1},
            {10, 1, 0, 10, 0, 6, 9, 5, 0, 5, 6, 0, -1, -1, -1, -1},
            {0, 3, 8, 5, 6, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {10, 5, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {11, 5, 10, 7, 5, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {11, 5, 10, 11, 7, 5, 8, 3, 0, -1, -1, -1, -1, -1, -1, -1},
            {5, 11, 7, 5, 10, 11, 1, 9, 0, -1, -1, -1, -1, -1, -1, -1},
            {10, 7, 5, 10, 11, 7, 9, 8, 1, 8, 3, 1, -1, -1, -1, -1},
            {11, 1, 2, 11, 7, 1, 7, 5, 1, -1, -1, -1, -1, -1, -1, -1},
            {0, 8, 3, 1, 2, 7, 1, 7, 5, 7, 2, 11, -1, -1, -1, -1},
            {9, 7, 5, 9, 2, 7, 9, 0, 2, 2, 11, 7, -1, -1, -1, -1},
            {7, 5, 2, 7, 2, 11, 5, 9, 2, 3, 2, 8, 9, 8, 2, -1},
            {2, 5, 10, 2, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1},
            {8, 2, 0, 8, 5, 2, 8, 7, 5, 10, 2, 5, -1, -1, -1, -1},
            {9, 0, 1, 5, 10, 3, 5, 3, 7, 3, 10, 2, -1, -1, -1, -1},
            {9, 8, 2, 9, 2, 1, 8, 7, 2, 10, 2, 5, 7, 5, 2, -1},
            {1, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {0, 8, 7, 0, 7, 1, 1, 7, 5, -1, -1, -1, -1, -1, -1, -1},
            {9, 0, 3, 9, 3, 5, 5, 3, 7, -1, -1, -1, -1, -1, -1, -1},
            {9, 8, 7, 5, 9, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {5, 8, 4, 5, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1},
            {5, 0, 4, 5, 11, 0, 5, 10, 11, 11, 3, 0, -1, -1, -1, -1},
            {0, 1, 9, 8, 4, 10, 8, 10, 11, 10, 4, 5, -1, -1, -1, -1},
            {10, 11, 4, 10, 4, 5, 11, 3, 4, 9, 4, 1, 3, 1, 4, -1},
            {2, 5, 1, 2, 8, 5, 2, 11, 8, 4, 5, 8, -1, -1, -1, -1},
            {0, 4, 11, 0, 11, 3, 4, 5, 11, 2, 11, 1, 5, 1, 11, -1},
            {0, 2, 5, 0, 5, 9, 2, 11, 5, 4, 5, 8, 11, 8, 5, -1},
            {9, 4, 5, 2, 11, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {2, 5, 10, 3, 5, 2, 3, 4, 5, 3, 8, 4, -1, -1, -1, -1},
            {5, 10, 2, 5, 2, 4, 4, 2, 0, -1, -1, -1, -1, -1, -1, -1},
            {3, 10, 2, 3, 5, 10, 3, 8, 5, 4, 5, 8, 0, 1, 9, -1},
            {5, 10, 2, 5, 2, 4, 1, 9, 2, 9, 4, 2, -1, -1, -1, -1},
            {8, 4, 5, 8, 5, 3, 3, 5, 1, -1, -1, -1, -1, -1, -1, -1},
            {0, 4, 5, 1, 0, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {8, 4, 5, 8, 5, 3, 9, 0, 5, 0, 3, 5, -1, -1, -1, -1},
            {9, 4, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {4, 11, 7, 4, 9, 11, 9, 10, 11, -1, -1, -1, -1, -1, -1, -1},
            {0, 8, 3, 4, 9, 7, 9, 11, 7, 9, 10, 11, -1, -1, -1, -1},
            {1, 10, 11, 1, 11, 4, 1, 4, 0, 7, 4, 11, -1, -1, -1, -1},
            {3, 1, 4, 3, 4, 8, 1, 10, 4, 7, 4, 11, 10, 11, 4, -1},
            {4, 11, 7, 9, 11, 4, 9, 2, 11, 9, 1, 2, -1, -1, -1, -1},
            {9, 7, 4, 9, 11, 7, 9, 1, 11, 2, 11, 1, 0, 8, 3, -1},
            {11, 7, 4, 11, 4, 2, 2, 4, 0, -1, -1, -1, -1, -1, -1, -1},
            {11, 7, 4, 11, 4, 2, 8, 3, 4, 3, 2, 4, -1, -1, -1, -1},
            {2, 9, 10, 2, 7, 9, 2, 3, 7, 7, 4, 9, -1, -1, -1, -1},
            {9, 10, 7, 9, 7, 4, 10, 2, 7, 8, 7, 0, 2, 0, 7, -1},
            {3, 7, 10, 3, 10, 2, 7, 4, 10, 1, 10, 0, 4, 0, 10, -1},
            {1, 10, 2, 8, 7, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {4, 9, 1, 4, 1, 7, 7, 1, 3, -1, -1, -1, -1, -1, -1, -1},
            {4, 9, 1, 4, 1, 7, 0, 8, 1, 8, 7, 1, -1, -1, -1, -1},
            {4, 0, 3, 7, 4, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {4, 8, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {9, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {3, 0, 9, 3, 9, 11, 11, 9, 10, -1, -1, -1, -1, -1, -1, -1},
            {0, 1, 10, 0, 10, 8, 8, 10, 11, -1, -1, -1, -1, -1, -1, -1},
            {3, 1, 10, 11, 3, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {1, 2, 11, 1, 11, 9, 9, 11, 8, -1, -1, -1, -1, -1, -1, -1},
            {3, 0, 9, 3, 9, 11, 1, 2, 9, 2, 11, 9, -1, -1, -1, -1},
            {0, 2, 11, 8, 0, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {3, 2, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {2, 3, 8, 2, 8, 10, 10, 8, 9, -1, -1, -1, -1, -1, -1, -1},
            {9, 10, 2, 0, 9, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {2, 3, 8, 2, 8, 10, 0, 1, 8, 1, 10, 8, -1, -1, -1, -1},
            {1, 10, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {1, 3, 8, 9, 1, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {0, 9, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {0, 3, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
            {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1} };

            /*
               Determine the index into the edge table which
               tells us which vertices are inside of the surface
            */
            cubeindex = 0;
            if (grid.val[0] < isolevel) cubeindex |= 1;
            if (grid.val[1] < isolevel) cubeindex |= 2;
            if (grid.val[2] < isolevel) cubeindex |= 4;
            if (grid.val[3] < isolevel) cubeindex |= 8;
            if (grid.val[4] < isolevel) cubeindex |= 16;
            if (grid.val[5] < isolevel) cubeindex |= 32;
            if (grid.val[6] < isolevel) cubeindex |= 64;
            if (grid.val[7] < isolevel) cubeindex |= 128;

            /* Cube is entirely in/out of the surface */
            if (edgeTable[cubeindex] == 0)
                return(0);

            /* Find the vertices where the surface intersects the cube */
            if (edgeTable[cubeindex] & 1)
                vertlist[0] =
                VertexInterp(isolevel, grid.p[0], grid.p[1], grid.val[0], grid.val[1]);
            if (edgeTable[cubeindex] & 2)
                vertlist[1] =
                VertexInterp(isolevel, grid.p[1], grid.p[2], grid.val[1], grid.val[2]);
            if (edgeTable[cubeindex] & 4)
                vertlist[2] =
                VertexInterp(isolevel, grid.p[2], grid.p[3], grid.val[2], grid.val[3]);
            if (edgeTable[cubeindex] & 8)
                vertlist[3] =
                VertexInterp(isolevel, grid.p[3], grid.p[0], grid.val[3], grid.val[0]);
            if (edgeTable[cubeindex] & 16)
                vertlist[4] =
                VertexInterp(isolevel, grid.p[4], grid.p[5], grid.val[4], grid.val[5]);
            if (edgeTable[cubeindex] & 32)
                vertlist[5] =
                VertexInterp(isolevel, grid.p[5], grid.p[6], grid.val[5], grid.val[6]);
            if (edgeTable[cubeindex] & 64)
                vertlist[6] =
                VertexInterp(isolevel, grid.p[6], grid.p[7], grid.val[6], grid.val[7]);
            if (edgeTable[cubeindex] & 128)
                vertlist[7] =
                VertexInterp(isolevel, grid.p[7], grid.p[4], grid.val[7], grid.val[4]);
            if (edgeTable[cubeindex] & 256)
                vertlist[8] =
                VertexInterp(isolevel, grid.p[0], grid.p[4], grid.val[0], grid.val[4]);
            if (edgeTable[cubeindex] & 512)
                vertlist[9] =
                VertexInterp(isolevel, grid.p[1], grid.p[5], grid.val[1], grid.val[5]);
            if (edgeTable[cubeindex] & 1024)
                vertlist[10] =
                VertexInterp(isolevel, grid.p[2], grid.p[6], grid.val[2], grid.val[6]);
            if (edgeTable[cubeindex] & 2048)
                vertlist[11] =
                VertexInterp(isolevel, grid.p[3], grid.p[7], grid.val[3], grid.val[7]);

            /* Create the triangle */
            ntriang = 0;
            for (i = 0; triTable[cubeindex][i] != -1; i += 3) {
                triangles[ntriang].p[0] = vertlist[triTable[cubeindex][i]];
                triangles[ntriang].p[1] = vertlist[triTable[cubeindex][i + 1]];
                triangles[ntriang].p[2] = vertlist[triTable[cubeindex][i + 2]];
                ntriang++;
            }

            return(ntriang);
        }
        /*
           Linearly interpolate the position where an isosurface cuts
           an edge between two vertices, each with their own scalar value
        */
        static FVector VertexInterp(float isolevel, FVector p1, FVector p2, float valp1, float valp2)
        {
            float mu;
            FVector p = FVector(0, 0, 0);

            if (abs(isolevel - valp1) < 0.00001)
            {
                return(p1);
            }
            if (abs(isolevel - valp2) < 0.00001)
            {
                return(p2);
            }
            if (abs(valp1 - valp2) < 0.00001)
            {
                return(p1);
            }

            mu = (isolevel - valp1) / (valp2 - valp1);

            p.X = p1.X + mu * (p2.X - p1.X);
            p.Y = p1.Y + mu * (p2.Y - p1.Y);
            p.Z = p1.Z + mu * (p2.Z - p1.Z);

            return(p);

        }
    };
    struct Node {
        bool empty = true;
        FVector location;
        bool generated = false;
        bool indexed = false;
        float radius, isolevel, node_size, gridcell_size;
        int index;
        std::vector<GRIDCELL> grid;
        static int id;
        static bool drawDebugBoxes;
        AMyPlanetActor* texturesOwner;
        Node(FVector _location, float _radius, float _isolevel, float _node_size, float _gridcell_size, AMyPlanetActor* _texturesOwner)
            :radius(_radius), isolevel(_isolevel), node_size(_node_size), gridcell_size(_gridcell_size), texturesOwner(_texturesOwner) {
            location = FVector(closestMultiple((int)_location.X, gridcell_size),
                closestMultiple((int)_location.Y, gridcell_size),
                closestMultiple((int)_location.Z, gridcell_size));
            index = Node::id;
            Node::id++;
        }
        void createGridcell(FVector _location) {
            GRIDCELL gridcell;
            indexGridcell(gridcell, _location);
            grid.push_back(gridcell);
        }
        void indexGrid() {
            if (indexed) return;
            for (float X = -node_size / 2.0; X <= node_size / 2.0; X += gridcell_size) {
                for (float Y = -node_size / 2.0; Y <= node_size / 2.0; Y += gridcell_size) {
                    for (float Z = -node_size / 2.0; Z <= node_size / 2.0; Z += gridcell_size) {
                        GRIDCELL gridcell;
                        indexGridcell(gridcell, FVector(X, Y, Z) + location);
                        grid.push_back(gridcell);
                    }
                } 

            }
            indexed = true;
        }
        void reindexGrid(FVector _location) {
            FVector lastLocation = location;
            location = FVector(closestMultiple((int)_location.X, gridcell_size),
                closestMultiple((int)_location.Y, gridcell_size),
                closestMultiple((int)_location.Z, gridcell_size));

            grid.clear();
            
            generated = false;
            for (float X = -node_size / 2.0; X <= node_size / 2.0; X += gridcell_size) {
                for (float Y = -node_size / 2.0; Y <= node_size / 2.0; Y += gridcell_size) {
                    for (float Z = -node_size / 2.0; Z <= node_size / 2.0; Z += gridcell_size) {
                        GRIDCELL gridcell;
                        indexGridcell(gridcell, FVector(X, Y, Z) + location);
                        grid.push_back(gridcell);
                    }
                }
            }
        }
        void indexGridcell(GRIDCELL& gridcell, FVector _location) {
            for (int i = 0; i < 8; i++) {
                FVector _loc;
                if (i == 0)
                    _loc = FVector(-0.5 * gridcell_size, -0.5 * gridcell_size, 0.5 * gridcell_size) + _location; //0
                else if (i == 1)
                    _loc = FVector(0.5 * gridcell_size, -0.5 * gridcell_size, 0.5 * gridcell_size) + _location; //0.5
                else if (i == 2)
                    _loc = FVector(0.5 * gridcell_size, -0.5 * gridcell_size, -0.5 * gridcell_size) + _location; //2
                else if (i == 3)
                    _loc = FVector(-0.5 * gridcell_size, -0.5 * gridcell_size, -0.5 * gridcell_size) + _location; //3
                else if (i == 4)
                    _loc = FVector(-0.5 * gridcell_size, 0.5 * gridcell_size, 0.5 * gridcell_size) + _location; //4
                else if (i == 5)
                    _loc = FVector(0.5 * gridcell_size, 0.5 * gridcell_size, 0.5 * gridcell_size) + _location; //5
                else if (i == 6)
                    _loc = FVector(0.5 * gridcell_size, 0.5 * gridcell_size, -0.5 * gridcell_size) + _location; //6
                else if (i == 7)
                    _loc = FVector(-0.5 * gridcell_size, 0.5 * gridcell_size, -0.5 * gridcell_size) + _location; //7

                FColor topography = texturesOwner->getTopographyAt(generateUV(_loc));
                FVector n = _loc;
                n.Normalize();
                gridcell.p.push_back(_loc);
                float offset = texturesOwner->HeightMultiplier* topography.R;
                double dist = FVector::Dist(FVector(0, 0, 0), _loc - n * offset);
                gridcell.val.push_back(radius - dist);
            }
        }
        static FVector2D generateUV(FVector pos) {
            FVector n = pos;
            n.Normalize(0.0);
            float u = (atan2(n.X, n.Z) / (2 * PI)) + 0.5;
            float v = 0.5 - (asin(n.Y) / PI);

            return FVector2D(u, v);
        }
        static FVector2D generateUV2(FVector pos) {
            FVector a_coords_n = pos;
            /*  a_coords_n.Normalize();
              a_coords_n *= -1;
              float u = 0.5 + atan2(a_coords_n.X, a_coords_n.Y) / (2.0 * PI);
              float v = 0.5 - asin(a_coords_n.Z) / PI;
              float offs = 0.001;

              return FVector2D(u, v);*/
            a_coords_n.Normalize(0.0);
            float lon = atan2(a_coords_n.X, a_coords_n.Z);
            float lat = acos(a_coords_n.Y);
            FVector2D sphereCoords = FVector2D(lon, lat) * (1.0 / PI);
            return FVector2D(sphereCoords.X * 0.5 + 0.5, 1.0 - sphereCoords.Y);

        }
        int generateCube(TArray<FVector>& vertices,
            TArray<int32>& Triangles,
            TArray<FVector>& normals,
            TArray<FVector2D>& UV0,
            TArray<FLinearColor>& vertexColor,
            TArray<FProcMeshTangent>& tangents)
        {
            if (generated) return 0;
            generated = true;
            FVector _vertices[8] =
            {
                FVector(-1, -1, -1),
                FVector(1, -1, -1),
                FVector(1, 1, -1),
                FVector(-1, 1, -1),
                FVector(-1, -1, 1),
                FVector(1, -1, 1),
                FVector(1, 1, 1),
                FVector(-1, 1, 1)
            };
            for (int i = 0; i < 8; i++) {
                _vertices[i] *= node_size / 2;
                _vertices[i] += FVector(location.X, location.Y, location.Z);
            }
            FVector2D _texCoords[4] =
            {
                FVector2D(0, 0),
                FVector2D(1, 0),
                FVector2D(1, 1),
                FVector2D(0, 1)
            };

            FVector _normals[6] =
            {
                FVector(0, 0, 1),
                FVector(1, 0, 0),
                FVector(0, 0, -1),
                FVector(-1, 0, 0),
                FVector(0, 1, 0),
                FVector(0, -1, 0)
            };

            int _indices[6 * 6] =
            {
                0, 1, 3, 3, 1, 2,
                1, 5, 2, 2, 5, 6,
                5, 4, 6, 6, 4, 7,
                4, 0, 7, 7, 0, 3,
                3, 2, 7, 7, 2, 6,
                4, 5, 0, 0, 5, 1
            };

            for (int i = 0; i < 36; i++) {
                _indices[i] += vertices.Num();
            }
            auto  get_random = []()
            {
                static std::default_random_engine e;
                static std::uniform_real_distribution<> dis(0, 1); // rage 0 - 1
                return dis(e);
            };
            FLinearColor linearColor(get_random(), get_random(), get_random(), 1.0);
            for (auto v : _vertices) {
                vertices.Add(FVector(v.X, v.Y, v.Z));
                vertexColor.Add(linearColor);
            }
            for (auto i : _indices)
                Triangles.Add(i);
            for (auto n : _normals)
                normals.Add(FVector(n.X, n.Y, n.Z));
            for (auto uv : _texCoords)
                UV0.Add(FVector2D(uv.X, uv.Y));
            return 1;
        }
        int generateMarchingCube(TArray<FVector>& vertices,
            TArray<int32>& Triangles,
            TArray<FVector>& normals,
            TArray<FVector2D>& UV0,
            TArray<FLinearColor>& vertexColor,
            TArray<FProcMeshTangent>& tangents,
            TArray<FVector4>& instances) {
            if (generated) return 0;
            generated = true;
            std::vector<TRIANGLE> localtriangles;
            for (auto gridcell : grid) {
                TRIANGLE _triangles[5];
                int ntriangles = Cube::Polygonise(gridcell, 0.0, _triangles);
                for (int i = 0; i < ntriangles; i++) {
                    localtriangles.push_back(_triangles[i]);
                }
            }
            int num_wert = 0;
            for (int i = 0; i < localtriangles.size(); i++) {
                if (vertexExists(vertices, localtriangles[i].p[2], 0.1) < 0) {
                    vertices.Add(localtriangles[i].p[2]);
                    Triangles.Add(vertices.Num()-1);
                }
                else {
                    Triangles.Add(vertexExists(vertices, localtriangles[i].p[2], 0.1));
                }

                if (vertexExists(vertices, localtriangles[i].p[1], 0.1) < 0) {
                    vertices.Add(localtriangles[i].p[1]);
                    Triangles.Add(vertices.Num() - 1);
                }
                else {
                    Triangles.Add(vertexExists(vertices, localtriangles[i].p[1], 0.1));
                }

                if (vertexExists(vertices, localtriangles[i].p[0], 0.1) < 0) {
                    vertices.Add(localtriangles[i].p[0]);
                    Triangles.Add(vertices.Num() - 1);
                }
                else {
                    Triangles.Add(vertexExists(vertices, localtriangles[i].p[0], 0.1));
                }
            }
            for(auto tree: texturesOwner->Trees) {
                tree->ClearInstances();
            }
            std::vector<int> foliagesTest;
            for (auto v : vertices) {
                std::vector<FVector> locs = randomPoints(getRandom(0, 20, v), v, getRandom(0, 100, v));
                for (auto loc : locs) {
                    FTransform InstanceTransform;
                    FColor topography = texturesOwner->getTopographyAt(generateUV(loc));
                    loc.Normalize();
                    loc = loc * (radius + texturesOwner->HeightMultiplier * topography.R);
                    InstanceTransform.SetLocation(loc);
                    FRotator MyRotator = FRotationMatrix::MakeFromZ(loc).Rotator();
                    InstanceTransform.SetRotation(MyRotator.Quaternion());
                    InstanceTransform.SetScale3D(FVector(0.1, 0.1, 0.1));
                    texturesOwner->Trees[int(getRandom(0, 8, loc))]->AddInstance(InstanceTransform);
                    foliagesTest.push_back(int(getRandom(0, 8, loc)));

                }

            }
            generated = true;

            return 1;
        }
        int generatePolygons(TArray<FVector>& vertices,
            TArray<int32>& Triangles,
            TArray<FVector>& normals,
            TArray<FVector2D>& UV0,
            TArray<FLinearColor>& vertexColor,
            TArray<FProcMeshTangent>& tangents,
            TArray<FVector4>& instances) {
            if (Node::drawDebugBoxes)
                return generateCube(vertices, Triangles, normals, UV0, vertexColor, tangents);
            else
                return generateMarchingCube(vertices, Triangles, normals, UV0, vertexColor, tangents, instances);
        }
        ~Node() {
            
        }
    };
    void freeNode(Node* node);
    void findPositionsToGenerateNodes();
    bool checkIfNodeExists(FVector position, float epsilon);
    FVector initialPosition;
    std::vector<Node*> nodes;
    std::vector<int> nodesToRemove;
    std::vector<FVector> positionsToGenerateNode;
    Node* collisionNode;
    bool bFirstCollisionNodeGeneration = true;
public:
    // Called every frame
    virtual void Tick(float DeltaTime) override;

};
