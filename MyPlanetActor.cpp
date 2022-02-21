// Fill out your copyright notice in the Description page of Project Settings.


#include "MyPlanetActor.h"
#include "ProceduralMeshComponent.h"
#include <thread>
#include <chrono>
int AMyPlanetActor::Node::id = 3;
bool AMyPlanetActor::Node::drawDebugBoxes = false;
AMyPlanetActor::AMyPlanetActor()
{
    MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("PlanetMesh"));
    static_cast<UProceduralMeshComponent*>(MeshComponent)->bUseAsyncCooking = false;
    static_cast<UProceduralMeshComponent*>(MeshComponent)->SetMeshSectionVisible(1, false);
    
}

// Called when the game starts or when spawned
void AMyPlanetActor::BeginPlay()
{
    FVector StartPos = GetWorld()->GetFirstPlayerController()->GetPawn()->GetActorLocation();
    Node::drawDebugBoxes = this->DrawDebugBoxes;
    if (DrawLowPoly) {
        generateSphere();
    }
    if(DrawMarchingCubes){
       // populatePlanetaryGeometry();
    }
    for (int i = 0; i < this->nodes.size(); i++) {
        nodesToIndex.push_back(i);
        //nodesToGenerateGeometry.push_back(i);
    }
    auto  get_random = []()
    {
        static std::default_random_engine e;
        e.seed(std::chrono::system_clock::now().time_since_epoch().count());
        static std::uniform_real_distribution<> dis(-1, 1); // rage 0 - 1
        return dis(e);
    };
    FVector pos = FVector(get_random(), get_random(), get_random());// *(Radius + 450.0);
    pos.Normalize();
    GetWorld()->GetFirstPlayerController()->GetPawn()->SetActorLocation(pos * (Radius + 50000.0));
    currentPawnPos = pos * (Radius);
    if(DrawMarchingCubes)
     addGridPoint(FVector4(currentPawnPos.X, currentPawnPos.Y, currentPawnPos.Z,1.0));

    std::thread* t = new std::thread(&AMyPlanetActor::launchNodeManagementLoop, this);
    t->detach();
    this->collisionNode = new Node(FVector(0, 0, 0), Radius, Isolevel,2000, 100, this);
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

void AMyPlanetActor::addGridPoint(FVector4 newgridpoint)
{
    for (auto gridpoint : gridPoints) {
        if (FVector::Distance(FVector(gridpoint), FVector(newgridpoint)) < 10.0) {
            return;
        }
    }
    gridPoints.push_back(newgridpoint);
}

void AMyPlanetActor::launchGenerationLoop()
{
    while (1) {
        for (auto n : nodes)
        {
            if (FVector::Distance(currentPawnPos, n->location) < IndexGridcellsDist) {
                n->indexGrid();
            }
            if (FVector::Distance(currentPawnPos, n->location) < GenerateGeometriesDist) {
                TArray<FVector> vertices;
                TArray<int32> Triangles;
                TArray<FVector> normals;
                TArray<FVector2D> UV0;
                TArray<FProcMeshTangent> tangents;
                TArray<FLinearColor> vertexColors;
                if (n->generatePolygons(vertices, Triangles, normals, UV0, vertexColors, tangents) == 1) {
                    static_cast<UProceduralMeshComponent*>(MeshComponent)->CreateMeshSection_LinearColor(n->index, vertices, Triangles, normals, UV0, vertexColors, tangents, true);
                    static_cast<UProceduralMeshComponent*>(MeshComponent)->ContainsPhysicsTriMeshData(true);
                    if (planetMaterial)
                        static_cast<UProceduralMeshComponent*>(MeshComponent)->SetMaterial(n->index, planetMaterial);
                }
            }
        }
    }
}

void AMyPlanetActor::launchNodeManagementLoop()
{
    managementMutex.lock();
    while (doManagementLoop) {
        std::vector<FVector> geometriesToPlace;
        for (FVector4& gridPoint : gridPoints) {
            if (FVector::Distance(currentPawnPos, FVector(gridPoint)) < PlaceNodeDist && gridPoint.W !=0 ) {
                geometriesToPlace.push_back(FVector(gridPoint.X, gridPoint.Y, gridPoint.Z));
                gridPoint.W = 0;
            }
        }
        for (auto gridPoint : geometriesToPlace) {
            nodesToIndex.push_back(this->nodes.size());
            placeGeometryNode(gridPoint);
        }
        if (!isGenerationReady) continue;
        for (int iToIndex : nodesToIndex) {
            if (FVector::Distance(currentPawnPos, this->nodes[iToIndex]->location) < IndexGridcellsDist && !this->nodes[iToIndex]->indexed) {
                indexIndexesToRemove.push_back(iToIndex);
                nodesToGenerateGeometry.push_back(iToIndex);
                isGenerationReady = false;
            }
        }
       
        for (int iToIndex : nodesToGenerateGeometry) {
            if (FVector::Distance(currentPawnPos, this->nodes[iToIndex]->location) <  GenerateGeometriesDist && !this->nodes[iToIndex]->generated) {
                geometryIndexesToRemove.push_back(iToIndex);
                isGenerationReady = false;
            }
        }
        /*for (int i = 0; i < nodes.size(); i++)
        {
            auto n = nodes.at(i);
            if (FVector::Distance(currentPawnPos, n->location) < IndexGridcellsDist) {
                //nodesToIndex.push_back(i);
                n->indexGrid();
            }
            if (FVector::Distance(currentPawnPos, n->location) < GenerateGeometriesDist) {
                nodesToGenerateGeometry.push_back(i);
            }
        }*/
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
    auto generateUV3 = [](FVector p) {
        float _radius = sqrt(p.X * p.X + p.Y * p.Y + p.Z * p.Z);
        float longitudeRadians = atan2(p.Y, p.X);
        float latitudeRadians = asin(p.Z / _radius);

        // Convert range -PI...PI to 0...1
        float s = longitudeRadians / (2 * PI) + 0.5;

        // Convert range -PI/2...PI/2 to 0...1
        float t = latitudeRadians / PI + 0.5;
        return FVector2D(s, t);

    };
    static_cast<UProceduralMeshComponent*>(MeshComponent)->CreateMeshSection_LinearColor(0, vertices, indices, normals, texCoords, vertexColors, tangents, false);
    static_cast<UProceduralMeshComponent*>(MeshComponent)->ContainsPhysicsTriMeshData(true);
    static_cast<UProceduralMeshComponent*>(MeshComponent)->GetBodySetup()->bDoubleSidedGeometry = true;
    if (planetMaterial)
        static_cast<UProceduralMeshComponent*>(MeshComponent)->SetMaterial(0, planetMaterial);
    if (vertexColorMaterial)
        static_cast<UProceduralMeshComponent*>(MeshComponent)->SetMaterial(0, vertexColorMaterial);
}

void AMyPlanetActor::populatePlanetaryGeometry()
{
    for (double X = -Radius; X <= Radius; X += GeometryNodeSize) {
        for (double Y = -Radius; Y <= Radius; Y += GeometryNodeSize) {
            for (double Z = -Radius; Z <= Radius; Z += GeometryNodeSize) {
                if (FVector::Distance(FVector(0, 0, 0), FVector(X, Y, Z)) > Radius*0.8  && FVector::Distance(FVector(0, 0, 0), FVector(X, Y, Z)) < Radius*1.2)
                    gridPoints.push_back(FVector4(X, Y, Z,1));
                    //placeGeometryNode(FVector(X, Y, Z));
            }
        }
    }
}

void AMyPlanetActor::placeGeometryNode(FVector location)
{
    this->nodes.push_back(new Node(location, Radius, Isolevel, GeometryNodeSize, NodeGridcellSize, this));
}

void AMyPlanetActor::generateGeometries(FVector currentLocation, float scaleFactor)
{
    throw "Generate geometries is not implemented";
}

// Called every frame
void AMyPlanetActor::Tick(float DeltaTime)
{
    for (int itoe : indexIndexesToRemove) {
        nodes[itoe]->indexGrid();
    }
    indexIndexesToRemove.clear();
    for (int itoe : geometryIndexesToRemove) {
        TArray<FVector> vertices;
        TArray<int32> Triangles;
        TArray<FVector> normals;
        TArray<FVector2D> UV0;
        TArray<FProcMeshTangent> tangents;
        TArray<FLinearColor> vertexColors;
        if (this->nodes[itoe]->generatePolygons(vertices, Triangles, normals, UV0, vertexColors, tangents) == 1) {
            if (vertices.Num() == 0) continue;
          /*  this->nodes[itoe]->vertices = vertices;
            this->nodes[itoe]->Triangles = Triangles;
            this->nodes[itoe]->normals = normals;
            this->nodes[itoe]->UV0 = UV0;
            this->nodes[itoe]->tangents = tangents;
            this->nodes[itoe]->vertexColors = vertexColors;*/
            this->nodes[itoe]->empty = false;
            for (float X = -GeometryNodeSize; X <= GeometryNodeSize; X += GeometryNodeSize) {
                for (float Y = -GeometryNodeSize; Y <= GeometryNodeSize; Y += GeometryNodeSize) {
                    for (float Z = -GeometryNodeSize; Z <= GeometryNodeSize; Z += GeometryNodeSize) {
                        if (X == 0 && Z == 0 && Y == 0) continue;
                        addGridPoint(FVector(nodes[itoe]->location.X + X, nodes[itoe]->location.Y + Y, nodes[itoe]->location.Z + Z));
                    }
                }
            }


            static_cast<UProceduralMeshComponent*>(MeshComponent)->CreateMeshSection_LinearColor(this->nodes[itoe]->index, vertices, Triangles, normals, UV0, vertexColors, tangents, false);
            static_cast<UProceduralMeshComponent*>(MeshComponent)->ContainsPhysicsTriMeshData(true);
            if (vertexColorMaterial)
                static_cast<UProceduralMeshComponent*>(MeshComponent)->SetMaterial(this->nodes[itoe]->index, vertexColorMaterial);
            else if (planetMaterial)
                static_cast<UProceduralMeshComponent*>(MeshComponent)->SetMaterial(this->nodes[itoe]->index, planetMaterial);
           // delete this->nodes[itoe];
           // this->nodes[itoe]->grid.clear();
        }
    }
    geometryIndexesToRemove.clear();
    isGenerationReady = true;
    FVector orig_currentPawnPos = GetWorld()->GetFirstPlayerController()->GetPawn()->GetActorLocation();
    currentPawnPos = orig_currentPawnPos;
    currentPawnPos.Normalize();
    currentPawnPos = currentPawnPos * Radius;
    for (auto node : nodes) {
        if(!node->empty){
            if (FVector::Distance(node->location, currentPawnPos) < 2.0 * GeometryNodeSize) {
               // MeshComponent->UpdateMeshSection_LinearColor(node->index, node->vertices, node->Triangles, node->normals, node->UV0, node->vertexColors, node->tangents, true);
               // MeshComponent->collision
            }
            else {

            }
        }
    }
    /*
    if (FVector::Distance(orig_currentPawnPos, FVector(0, 0, 0)) < Radius * 1.1) {
        MeshComponent->SetMeshSectionVisible(0, false);
    }
    else {
        MeshComponent->SetMeshSectionVisible(0, true);
    }
    */


    TArray<FVector> vertices;
    TArray<int32> Triangles;
    TArray<FVector> normals;
    TArray<FVector2D> UV0;
    TArray<FProcMeshTangent> tangents;
    TArray<FLinearColor> vertexColors;
    //((number + multiple/2) / multiple) * multiple;
    //((currentPawnPos.X+GeometryNodeSize/2)/GeometryNodeSize)*GeometryNodeSize
    auto closestMultiple = [](int n, int x)
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
    this->collisionNode->reindexGrid(FVector(
        closestMultiple((int)currentPawnPos.X, NodeGridcellSize),
        closestMultiple((int)currentPawnPos.Y, NodeGridcellSize),
        closestMultiple((int)currentPawnPos.Z, NodeGridcellSize)
    ));
    
    if (this->collisionNode->generatePolygons(vertices, Triangles, normals, UV0, vertexColors, tangents) == 1) {
        static_cast<UProceduralMeshComponent*>(MeshComponent)->CreateMeshSection_LinearColor(1, vertices, Triangles, normals, UV0, vertexColors, tangents, true);        
        static_cast<UProceduralMeshComponent*>(MeshComponent)->ContainsPhysicsTriMeshData(true);
        static_cast<UProceduralMeshComponent*>(MeshComponent)->SetMeshSectionVisible(1, true);
    }
    Super::Tick(DeltaTime);
}

