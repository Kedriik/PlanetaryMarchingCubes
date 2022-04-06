// Fill out your copyright notice in the Description page of Project Settings.
#include "MyPlanetActor.h"
#include "ProceduralMeshComponent.h"
#include <thread>
#include <string>
#include <chrono>
int AMyPlanetActor::Node::id = 3;
bool AMyPlanetActor::Node::drawDebugBoxes = false;
AMyPlanetActor::AMyPlanetActor()
{
    MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("PlanetMesh"));
    static_cast<UProceduralMeshComponent*>(MeshComponent)->bUseAsyncCooking = false;
    static_cast<UProceduralMeshComponent*>(MeshComponent)->SetMeshSectionVisible(1, false);
    for (int i = 0; i < this->Trees.Num(); i++) {
        FName n((std::string("Foliage") + std::to_string(i)).c_str());
        Trees[i] = CreateDefaultSubobject<UFoliageInstancedStaticMeshComponent>(n);
    }
    //this->Trees = CreateDefaultSubobject<UFoliageInstancedStaticMeshComponent>(TEXT("UFoliageInstancedStaticMeshComponent"));
    //SetRootComponent(this->hismc);
    
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
        static std::uniform_real_distribution<> dis(-1, 1); // rage 0 - 1
        return dis(e);
    };
    FVector pos = FVector(get_random(), get_random(), get_random());
    pos.Normalize();
    GetWorld()->GetFirstPlayerController()->GetPawn()->SetActorLocation(pos * (Radius + 0.02*Radius));
    currentPawnPos = pos * (Radius);
   /* int instances = 0;
    for (int i = 0; i < instances; i++) {
        FTransform InstanceTransform;
        FVector _loc = FVector(get_random(), get_random(), get_random());
        _loc.Normalize();
        _loc *= Radius;
        InstanceTransform.SetLocation(_loc);
        FRotator MyRotator = FRotationMatrix::MakeFromZ(_loc).Rotator();
        InstanceTransform.SetRotation(MyRotator.Quaternion());
        this->hismc->AddInstance(InstanceTransform);
        //this->hismc->ClearInstances();
    }*/
    
    
    initialPosition = currentPawnPos;
    lastCollisionMeshUpdate = FVector(0,0,0);
    for (float X = -RenderDistance ; X < RenderDistance; X += GeometryNodeSize) {
        for (float Y = -RenderDistance; Y < RenderDistance; Y += GeometryNodeSize) {
            for (float Z = -RenderDistance; Z < RenderDistance; Z += GeometryNodeSize) {
                //placeAndGenerateNode(FVector(X, Y, Z) + currentPawnPos);
            }
        }
    }
 //   std::thread* t = new std::thread(&AMyPlanetActor::launchNodeManagementLoop, this);
//    t->detach();

    this->collisionNode = new Node(FVector(0, 0, 0), Radius, Isolevel, GeometryNodeSize, NodeGridcellSize, this);
    Super::BeginPlay();
}

void AMyPlanetActor::BeginDestroy()
{
    doManagementLoop = false;
    managementMutex.lock();
    delete collisionNode;
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
        for (int i = nodes.size() - 1; i >= 0; i--) {
            if (FVector::Distance(nodes.at(i)->location, currentPawnPos) > 1.2*RenderDistance) {
                nodesToRemove.push_back(i);
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    managementMutex.unlock();
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

       /* for (int i = 0; i < indices.Num(); i++) {
            FVector n = vertices[indices[i]];
            FColor topography = this->getTopographyAt(Node::generateUV(n));
            n.Normalize();
            float offset = (this->HeightMultiplier/5.0) * topography.R;
            vertices[indices[i]] +=  n * offset;

        }*/
    }

    TArray<FProcMeshTangent> tangents;
    static_cast<UProceduralMeshComponent*>(MeshComponent)->CreateMeshSection_LinearColor(0, vertices, indices, normals, texCoords, vertexColors, tangents, false);
    static_cast<UProceduralMeshComponent*>(MeshComponent)->GetBodySetup()->bDoubleSidedGeometry = true;
    if (planetMaterial)
        static_cast<UProceduralMeshComponent*>(MeshComponent)->SetMaterial(0, planetMaterial);
    if (vertexColorMaterial)
        static_cast<UProceduralMeshComponent*>(MeshComponent)->SetMaterial(0, vertexColorMaterial);
}

void AMyPlanetActor::placeGeometryNode(FVector location)
{
    this->nodes.push_back(new Node(location, Radius, Isolevel, GeometryNodeSize, NodeGridcellSize, this));
}

void AMyPlanetActor::placeAndGenerateNode(FVector location)
{
    Node* n = new Node(location, Radius, Isolevel, GeometryNodeSize, NodeGridcellSize, this);

    TArray<FVector> vertices;
    TArray<int32> Triangles;
    TArray<FVector> normals;
    TArray<FVector4> instances;
    TArray<FVector2D> UV0;
    TArray<FProcMeshTangent> tangents;
    TArray<FLinearColor> vertexColors;

    n->indexGrid();

    if (n->generatePolygons(vertices, Triangles, normals, UV0, vertexColors, tangents, instances) == 1) {
        static_cast<UProceduralMeshComponent*>(MeshComponent)->CreateMeshSection_LinearColor(n->index, vertices, Triangles, normals, UV0, vertexColors, tangents, false);
       // static_cast<UProceduralMeshComponent*>(MeshComponent)->ContainsPhysicsTriMeshData(true);
        static_cast<UProceduralMeshComponent*>(MeshComponent)->SetMeshSectionVisible(n->index, true);
        static_cast<UProceduralMeshComponent*>(MeshComponent)->SetMaterial(n->index, planetMaterial);
        this->nodes.push_back(n);
    }
    else {
        delete n;
    }

}
void AMyPlanetActor::freeNode(Node* node)
{
    static_cast<UProceduralMeshComponent*>(MeshComponent)->ClearMeshSection(node->index);
    delete node;
    node = nullptr;
}
void AMyPlanetActor::findPositionsToGenerateNodes()
{
}
bool AMyPlanetActor::checkIfNodeExists(FVector position, float epsilon)
{
    for (auto node : nodes) {
        if(FVector::Distance(node->location, position)<epsilon)
            return true;
    }
        return false;
}
// Called every frame
void AMyPlanetActor::Tick(float DeltaTime)
{
    FVector orig_currentPawnPos = GetWorld()->GetFirstPlayerController()->GetPawn()->GetActorLocation();
    currentPawnPos = orig_currentPawnPos;
    currentPawnPos.Normalize();
    currentPawnPos = currentPawnPos * Radius;

    for (auto ntr : nodesToRemove) {
        freeNode(nodes.at(ntr));
        nodes.erase(nodes.begin() + ntr);
    }
    nodesToRemove.clear();
    TArray<FVector> vertices;
    TArray<int32> Triangles;
    TArray<FVector> normals;
    TArray<FVector4> instances;
    TArray<FVector2D> UV0;
    TArray<FProcMeshTangent> tangents;
    TArray<FLinearColor> vertexColors;
    if(FVector::Distance(currentPawnPos,lastCollisionMeshUpdate)>2.0* NodeGridcellSize){
        this->collisionNode->reindexGrid(currentPawnPos);
        
        if (this->collisionNode->generatePolygons(vertices, Triangles, normals, UV0, vertexColors, tangents,instances) == 1) {
            static_cast<UProceduralMeshComponent*>(MeshComponent)->CreateMeshSection_LinearColor(1, vertices, Triangles, normals, UV0, vertexColors, tangents, true);        
            //static_cast<UMeshComponent*>(MeshComponent)->friction
            static_cast<UProceduralMeshComponent*>(MeshComponent)->ContainsPhysicsTriMeshData(true);
            static_cast<UProceduralMeshComponent*>(MeshComponent)->SetMeshSectionVisible(1, true);
            static_cast<UProceduralMeshComponent*>(MeshComponent)->SetMaterial(1, planetMaterial);
        }
        lastCollisionMeshUpdate = currentPawnPos;
    }
    Super::Tick(DeltaTime);
}

