// Fill out your copyright notice in the Description page of Project Settings.
#include "MyPlanetActor.h"
//#include "ProceduralMeshComponent.h"
#include <thread>
#include <string>
#include <chrono>
int AMyPlanetActor::Node::id = 3;
std::mutex AMyPlanetActor::Node::id_mutex;
bool AMyPlanetActor::Node::drawDebugBoxes = false;
AMyPlanetActor::AMyPlanetActor()
{
    getTopographyAt(FVector2D(0, 0));
    MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("PlanetMesh"));
    static_cast<UProceduralMeshComponent*>(MeshComponent)->bUseAsyncCooking = false;
    static_cast<UProceduralMeshComponent*>(MeshComponent)->SetMeshSectionVisible(1, false);
    for (int i = 0; i < this->Trees.Num(); i++) {
        FName n((std::string("Foliage") + std::to_string(i)).c_str());
        Trees[i] = CreateDefaultSubobject<UFoliageInstancedStaticMeshComponent>(n);
    }
    for (int i = 0; i < this->Grass.Num(); i++) {
        FName n((std::string("FoliageGrass") + std::to_string(i)).c_str());
        Grass[i] = CreateDefaultSubobject<UFoliageInstancedStaticMeshComponent>(n);
    }
    
}
// Called when the game starts or when spawned
void AMyPlanetActor::BeginPlay()
{
    getTopographyAt(FVector2D(0,0));
    FVector StartPos = GetWorld()->GetFirstPlayerController()->GetPawn()->GetActorLocation();
    Node::drawDebugBoxes = this->DrawDebugBoxes;
    if (DrawLowPoly) {
        generateSphere();
    }
    auto  get_random = []()
    {
        static std::default_random_engine e;
        e.seed(std::chrono::system_clock::now().time_since_epoch().count());
        static std::uniform_real_distribution<> dis(-1, 1);
        return dis(e);
    };
    FVector pos = FVector(get_random(), get_random(), get_random());
    pos.Normalize();
    GetWorld()->GetFirstPlayerController()->GetPawn()->SetActorLocation(pos * (Radius + 0.02*Radius));
    currentPawnPos = pos * (Radius);initialPosition = currentPawnPos;
    lastCollisionMeshUpdate = FVector(0,0,0);
    findPositionsToGenerateNodes();
    /*
    for (int i = planetaryGridcells.size() - 1; i >= 0; i--) {
        if (FVector::Distance(planetaryGridcells.at(i)->location, currentPawnPos) < IndexGridcellsDist) {
            planetaryGridcells.at(i)->doAsyncWork();
        }
        if (FVector::Distance(planetaryGridcells.at(i)->location, currentPawnPos) < PlaceNodeDist && !planetaryGridcells.at(i)->node_placed) {
            planetaryGridcells_generateMesh.push_back(planetaryGridcells.at(i));
        }
    }
    */

    std::thread* t = new std::thread(&AMyPlanetActor::launchNodeManagementLoop, this);
    t->detach();
    Super::BeginPlay();
}

void AMyPlanetActor::BeginDestroy()
{
    doManagementLoop = false;
    managementMutex.lock();
    //delete collisionNode;
    Super::BeginDestroy();
}

void AMyPlanetActor::PostActorCreated()
{
    if (DrawLowPoly) {
        generateSphere();
       // DrawLowPoly = false;
    }
    Super::PostActorCreated();
}

void AMyPlanetActor::PostLoad()
{
    if (DrawLowPoly) 
        generateSphere();
    Super::PostLoad();
}

void AMyPlanetActor::launchNodeManagementLoop()
{
    managementMutex.lock();
    while (doManagementLoop) {
        for (int i = planetaryGridcells.size() - 1; i >= 0; i--) {
            if (FVector::Distance(planetaryGridcells.at(i)->location, currentPawnPos) < IndexGridcellsDist && !planetaryGridcells.at(i)->async_work_buffered) {
                planetaryGridcells.at(i)->doAsyncWork();
            }
        }
        for (int i = planetaryGridcells.size() - 1; i >= 0; i--) {
            if (planetaryGridcells.at(i)->geometry_node != nullptr &&
                FVector::Distance(planetaryGridcells.at(i)->location, currentPawnPos) < PlaceNodeDist && 
                !planetaryGridcells.at(i)->node_placed) {
                planetaryGridcells_generateMesh.push_back(planetaryGridcells.at(i));
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    managementMutex.unlock();
}

void AMyPlanetActor::launchPregeneratedManagementLoop()
{
}

void AMyPlanetActor::generateSphere()
{
    float sphereScalingFactor = 1.0;
    TArray<FVector> vertices;
    TArray<FVector> normals;
    TArray<FVector2D> texCoords;

    float x, y, z, xy;                              // vertex position
    float nx, ny, nz, lengthInv = 1.0f / (Radius * sphereScalingFactor);    // vertex normal
    float s, t;                                     // vertex texCoord

    float sectorStep = 2 * PI / sectorCount;
    float stackStep = PI / stackCount;
    float sectorAngle, stackAngle;

    for (int i = 0; i <= stackCount; ++i)
    {
        stackAngle = PI / 2 - i * stackStep;        // starting from pi/2 to -pi/2
        xy = (Radius * sphereScalingFactor) * cosf(stackAngle);             // r * cos(u)
        z = (Radius * sphereScalingFactor) * sinf(stackAngle);              // r * sin(u)

        // add (sectorCount+1) vertices per stack
        // the first and last vertices have same position and normal, but different tex coords
        for (int j = 0; j <= sectorCount; ++j)
        {
            sectorAngle = j * sectorStep;           // starting from 0 to 2pi

            // vertex position (x, y, z)
            x = xy * cosf(sectorAngle);             // r * cos(u) * cos(v)
            y = xy * sinf(sectorAngle);             // r * cos(u) * sin(v)
            vertices.Add(FVector(x, y, z));

            // normalized vertex normal (nx, ny, nz)
            nx = x * lengthInv;
            ny = y * lengthInv;
            nz = z * lengthInv;
            normals.Add(FVector(nx, ny, nz));

            // vertex tex coord (s, t) range between [0, 1]
            s = (float)j / sectorCount;
            t = (float)i / stackCount;
            // texCoords.Add(FVector2D(s,t));
        }
    }

    TArray<int32> indices;
    TArray<int> lineIndices;
    int k1, k2;
    for (int i = 0; i < stackCount; ++i)
    {
        k1 = i * (sectorCount + 1);     // beginning of current stack
        k2 = k1 + sectorCount + 1;      // beginning of next stack

        for (int j = 0; j < sectorCount; ++j, ++k1, ++k2)
        {
            // 2 triangles per sector excluding first and last stacks
            // k1 => k2 => k1+1
            if (i != 0)
            {

                indices.Add(k1 + 1);
                indices.Add(k2);
                indices.Add(k1);
            }

            // k1+1 => k2 => k2+1
            if (i != (stackCount - 1))
            {
                indices.Add(k2 + 1);
                indices.Add(k2);
                indices.Add(k1 + 1);
            }

            // store indices for lines
            // vertical lines for all stacks, k1 => k2
            lineIndices.Add(k1);
            lineIndices.Add(k2);
            if (i != 0)  // horizontal lines except 1st stack, k1 => k+1
            {
                lineIndices.Add(k1);
                lineIndices.Add(k1 + 1);
            }
        }
    }

    TArray<FLinearColor> vertexColors;
    if (PlanetTopography) {
        for (auto v : vertices) {
//            float R = getPixelAt(PlanetTopography, Node::generateUV(v)).R;//Topography::MarsTopography[Y][X];
//            texCoords.Add(Node::generateUV(v));
 //           vertexColors.Add(FLinearColor(R, R / 255.0, R / 255.0, 1.0));
        }

        for (int i = 0; i < indices.Num(); i++) {
            //FVector n = vertices[indices[i]];
            //FColor topography = this->getTopographyAt(Node::generateUV(n));
            //n.Normalize();
            //float offset = (this->HeightMultiplier/5.0) * topography.R;
            //vertices[indices[i]] +=  n * offset;

        }
    }

    TArray<FProcMeshTangent> tangents;
    static_cast<UProceduralMeshComponent*>(MeshComponent)->CreateMeshSection_LinearColor(0, vertices, indices, normals, texCoords, vertexColors, tangents, false);
    static_cast<UProceduralMeshComponent*>(MeshComponent)->GetBodySetup()->bDoubleSidedGeometry = true;
    if (planetMaterial)
        static_cast<UProceduralMeshComponent*>(MeshComponent)->SetMaterial(0, planetMaterial);
    if (vertexColorMaterial)
        static_cast<UProceduralMeshComponent*>(MeshComponent)->SetMaterial(0, vertexColorMaterial);
}

void AMyPlanetActor::pregenerateNodes()
{
    Node* node = new Node(FVector(0, 0, 0), Radius, Isolevel, NodeSize, GridcellSize, this);
    float margin = 1.01;
    for (float X = -Radius * margin; X <= Radius * margin; X += NodeSize) {
        for (float Y = -Radius * margin; Y <= Radius * margin; Y += NodeSize) {
            for (float Z = -Radius * margin; Z <= Radius * margin; Z += NodeSize) {
                if (FVector::Distance(FVector(X, Y, Z), FVector(0, 0, 0)) < Radius * (1.0 - (margin - 1.0))) {
                    continue;
                }
                else {
                    Node* n = Node::save(FVector(X, Y, Z), this);
                    //if(n != nullptr)
                        //nodes.push_back(n);
                }
            }
        }
    }
}

void AMyPlanetActor::placeGeometryNode(FVector location)
{
   
}

void AMyPlanetActor::placeAndGenerateNode(FVector location)
{
}

void AMyPlanetActor::freeNode(Node* node)
{
    static_cast<UProceduralMeshComponent*>(MeshComponent)->ClearMeshSection(node->index);
    delete node;
    node = nullptr;
}

void AMyPlanetActor::findPositionsToGenerateNodes()
{
    float margin =0.5;
    float _Radius = Radius;// *0.5;
    for (float X = -_Radius; X <= _Radius; X += NodeSize) {
        for (float Y = -_Radius; Y <= _Radius; Y += NodeSize) {
            for (float Z = -_Radius; Z <= _Radius; Z += NodeSize) {
               // if(FVector::Distance(FVector(0),FVector(X,Y,Z))>Radius-margin*NodeSize)
                    planetaryGridcells.push_back(new PlanetaryGridcell(this, FVector(X, Y, Z)));
            }
        }
    }
}
// Called every frame
void AMyPlanetActor::Tick(float DeltaTime)
{
    FVector orig_currentPawnPos = GetWorld()->GetFirstPlayerController()->GetPawn()->GetActorLocation();
    currentPawnPos = orig_currentPawnPos;// +GetWorld()->GetFirstPlayerController()->GetPawn()->GetActorForwardVector() * 0.5 * NodeSize;
   // currentPawnPos.Normalize();
    //currentPawnPos = currentPawnPos * Radius;
    for (auto gc : planetaryGridcells_generateMesh) {
        gc->placeNode();
        gc->freeMeshData();
    }
    planetaryGridcells_generateMesh.clear();
    Super::Tick(DeltaTime);
}

