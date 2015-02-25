#include "builders.h"

#include "tesselator.h"
#include "rectangle.h"
#include "geom.h"

#include <memory>

using Builders::CapTypes;
using Builders::JoinTypes;
using Builders::PolyLineOptions;
using Builders::PolygonOutput;
using Builders::PolyLineOutput;
using Builders::NO_TEXCOORDS;
using Builders::NO_SCALING_VECS;

void* alloc(void* _userData, unsigned int _size) {
    return malloc(_size);
}

void* realloc(void* _userData, void* _ptr, unsigned int _size) {
    return realloc(_ptr, _size);
}

void free(void* _userData, void* _ptr) {
    free(_ptr);
}

static TESSalloc allocator = {&alloc, &realloc, &free, nullptr,
                              64, // meshEdgeBucketSize
                              64, // meshVertexBucketSize
                              16,  // meshFaceBucketSize
                              64, // dictNodeBucketSize
                              16,  // regionBucketSize
                              64  // extraVertices
                             };

void Builders::buildPolygon(const Polygon& _polygon, PolygonOutput& _out) {
    
    TESStesselator* tesselator = tessNewTess(&allocator);
    
    bool useTexCoords = (&_out.texcoords != &NO_TEXCOORDS);
    
    // get the number of vertices already added
    int vertexDataOffset = (int)_out.points.size();
    
    Rectangle bBox;
    
    if (useTexCoords) {
        // initialize the axis-aligned bounding box of the polygon
        if(_polygon.size() > 0) {
            if(_polygon[0].size() > 0) {
                bBox.set(_polygon[0][0].x, _polygon[0][0].y, 0, 0);
            }
        }
    }
    
    // add polygon contour for every ring
    for (auto& line : _polygon) {
        if (useTexCoords) {
            bBox.growToInclude(line);
        }
        tessAddContour(tesselator, 3, line.data(), sizeof(Point), (int)line.size());
    }
    
    // call the tesselator
    glm::vec3 normal(0.0, 0.0, 1.0);
    
    if( tessTesselate(tesselator, TessWindingRule::TESS_WINDING_NONZERO, TessElementType::TESS_POLYGONS, 3, 3, &normal[0]) ) {
        
        const int numElements = tessGetElementCount(tesselator);
        const TESSindex* tessElements = tessGetElements(tesselator);
        _out.indices.reserve(_out.indices.size() + numElements * 3); // Pre-allocate index vector
        for(int i = 0; i < numElements; i++) {
            const TESSindex* tessElement = &tessElements[i * 3];
            for(int j = 0; j < 3; j++) {
                _out.indices.push_back(tessElement[j] + vertexDataOffset);
            }
        }
        
        const int numVertices = tessGetVertexCount(tesselator);
        const float* tessVertices = tessGetVertices(tesselator);
        _out.points.reserve(_out.points.size() + numVertices); // Pre-allocate vertex vector
        _out.normals.reserve(_out.normals.size() + numVertices); // Pre-allocate normal vector
        if (useTexCoords) {
            _out.texcoords.reserve(_out.texcoords.size() + numVertices); // Pre-allocate texcoord vector
        }
        for(int i = 0; i < numVertices; i++) {
            if (useTexCoords) {
                float u = mapValue(tessVertices[3*i], bBox.getMinX(), bBox.getMaxX(), 0., 1.);
                float v = mapValue(tessVertices[3*i+1], bBox.getMinY(), bBox.getMaxY(), 0., 1.);
                _out.texcoords.push_back(glm::vec2(u, v));
            }
            _out.points.push_back(glm::vec3(tessVertices[3*i], tessVertices[3*i+1], tessVertices[3*i+2]));
            _out.normals.push_back(normal);
        }
    }
    else {
        logMsg("Tesselator cannot tesselate!!\n");
    }
    
    tessDeleteTess(tesselator);
}

void Builders::buildPolygonExtrusion(const Polygon& _polygon, const float& _minHeight, PolygonOutput& _out) {
    
    int vertexDataOffset = (int)_out.points.size();
    
    glm::vec3 upVector(0.0f, 0.0f, 1.0f);
    glm::vec3 normalVector;
    
    bool useTexCoords = (&_out.texcoords != &NO_TEXCOORDS);
    
    for (auto& line : _polygon) {
        
        size_t lineSize = line.size();
        _out.points.reserve(_out.points.size() + lineSize * 4); // Pre-allocate vertex vector
        _out.normals.reserve(_out.normals.size() + lineSize * 4); // Pre-allocate normal vector
        _out.indices.reserve(_out.indices.size() + lineSize * 6); // Pre-allocate index vector
        if (useTexCoords) {
            _out.texcoords.reserve(_out.texcoords.size() + lineSize * 4); // Pre-allocate texcoord vector
        }
        
        for (size_t i = 0; i < lineSize - 1; i++) {
            
            normalVector = glm::cross(upVector, (line[i+1] - line[i]));
            normalVector = glm::normalize(normalVector);
            
            // 1st vertex top
            _out.points.push_back(line[i]);
            _out.normals.push_back(normalVector);
            
            // 2nd vertex top
            _out.points.push_back(line[i+1]);
            _out.normals.push_back(normalVector);
            
            // 1st vertex bottom
            _out.points.push_back(glm::vec3(line[i].x, line[i].y, _minHeight));
            _out.normals.push_back(normalVector);
            
            // 2nd vertex bottom
            _out.points.push_back(glm::vec3(line[i+1].x, line[i+1].y, _minHeight));
            _out.normals.push_back(normalVector);
            
            //Start the index from the previous state of the vertex Data
            _out.indices.push_back(vertexDataOffset);
            _out.indices.push_back(vertexDataOffset + 1);
            _out.indices.push_back(vertexDataOffset + 2);
            
            _out.indices.push_back(vertexDataOffset + 1);
            _out.indices.push_back(vertexDataOffset + 3);
            _out.indices.push_back(vertexDataOffset + 2);
            
            if (useTexCoords) {
                _out.texcoords.push_back(glm::vec2(1.,0.));
                _out.texcoords.push_back(glm::vec2(0.,0.));
                _out.texcoords.push_back(glm::vec2(1.,1.));
                _out.texcoords.push_back(glm::vec2(0.,1.));
            }
            
            vertexDataOffset += 4;
            
        }
    }
}

// Get 2D perpendicular of two points
glm::vec2 perp2d(const glm::vec3& _v1, const glm::vec3& _v2 ){
    return glm::vec2(_v2.y - _v1.y, _v1.x - _v2.x);
}

// Get 2D vector rotated 
glm::vec2 rotate(const glm::vec2& _v, float _radians) {
    float cos = std::cos(_radians);
    float sin = std::sin(_radians);
    return glm::vec2(_v.x * cos - _v.y * sin, _v.x * sin + _v.y * cos);
}

// Helper function for polyline tesselation
void addPolyLineVertex(const glm::vec3& _coord, const glm::vec2& _normal, const glm::vec2& _uv, float _halfWidth, PolyLineOutput _out) {

    if (&_out.scalingVecs != &NO_SCALING_VECS) {
        _out.points.push_back(_coord);
        _out.scalingVecs.push_back(_normal);
    } else {
        _out.points.push_back(glm::vec3( _coord.x + _normal.x * _halfWidth, _coord.y + _normal.y * _halfWidth, _coord.z));
    }

    if(&_out.texcoords != &NO_TEXCOORDS){
         _out.texcoords.push_back(_uv);
    }
}

// Helper function for polyline tesselation; adds indices for pairs of vertices arranged like a line strip
void indexPairs( int _nPairs, int _nVertices, std::vector<int>& _indicesOut) {
    for (int i = 0; i < _nPairs; i++) {
        _indicesOut.push_back(_nVertices - 2*i - 4);
        _indicesOut.push_back(_nVertices - 2*i - 2);
        _indicesOut.push_back(_nVertices - 2*i - 3);
        
        _indicesOut.push_back(_nVertices - 2*i - 3);
        _indicesOut.push_back(_nVertices - 2*i - 2);
        _indicesOut.push_back(_nVertices - 2*i - 1);
    }
}

//  Tessalate a fan geometry between points A       B
//  using their normals from a center        \ . . /
//  and interpolating their UVs               \ p /
//                                             \./
//                                              C
void addFan(const glm::vec3& _C, const glm::vec2& _CA, const glm::vec2& _CB, const glm::vec2& _uv,
             int _numTriangles, float _halfWidth, PolyLineOutput _out) {
    
    // Find angle difference
    float cross = _CA.x * _CB.y - _CA.y * _CB.x; // z component of cross(_CA, _CB)
    float angle = atan2f(cross, glm::dot(_CA, _CB));
    float dAngle = angle / (float)_numTriangles;
    
    int startIndex = _out.points.size();
    
    // Add center vertex
    addPolyLineVertex(_C, {0.0f, 0.0f}, _uv, _halfWidth, _out);
    
    // Add vertex for point A
    addPolyLineVertex(_C, _CA, _uv, _halfWidth, _out);
    
    // Add radial vertices
    glm::vec2 radial = _CA;
    for (int i = 0; i < _numTriangles; i++) {
        radial = rotate(radial, dAngle);
        addPolyLineVertex(_C, radial, _uv, _halfWidth, _out);
    }
    
    // Add indices
    for (int i = 0; i < _numTriangles; i++) {
        _out.indices.push_back(startIndex); // center vertex
        if (angle > 0) {
            _out.indices.push_back(startIndex + i + 2);
            _out.indices.push_back(startIndex + i + 1);
        } else {
            _out.indices.push_back(startIndex + i + 1);
            _out.indices.push_back(startIndex + i + 2);
        }
    }
    
}

// Function to add the vertices for line caps
void addCap(const glm::vec3& _coord, const glm::vec2& _normal, int _numCorners, bool _isBeginning, float _halfWidth, PolyLineOutput _out) {

    if (_numCorners < 1) {
        return;
    }
    
    glm::vec2 uv = glm::vec2(0.5, _isBeginning ? 0.0 : 1.0); // center point UVs
    float sign = _isBeginning ? 1.0f : -1.0f; // caps at beginning and end fan in opposite directions

    addFan(_coord, -sign * _normal, sign * _normal, uv, _numCorners * 2, _halfWidth, _out);
}

float valuesWithinTolerance(float _a, float _b, float _tolerance = 0.001) {
    return fabs(_a - _b) < _tolerance;
}

// Tests if a line segment (from point A to B) is nearly coincident with the edge of a tile
bool isOnTileEdge(const glm::vec3& _pa, const glm::vec3& _pb) {
    
    float tolerance = 0.0002; // tweak this adjust if catching too few/many line segments near tile edges
    // TODO: make tolerance configurable by source if necessary
    glm::vec2 tile_min = glm::vec2(-1.0,-1.0);
    glm::vec2 tile_max = glm::vec2(1.0,1.0);

    return (valuesWithinTolerance(_pa.x, tile_min.x, tolerance) && valuesWithinTolerance(_pb.x, tile_min.x, tolerance)) ||
           (valuesWithinTolerance(_pa.x, tile_max.x, tolerance) && valuesWithinTolerance(_pb.x, tile_max.x, tolerance)) ||
           (valuesWithinTolerance(_pa.y, tile_min.y, tolerance) && valuesWithinTolerance(_pb.y, tile_min.y, tolerance)) ||
           (valuesWithinTolerance(_pa.y, tile_max.y, tolerance) && valuesWithinTolerance(_pb.y, tile_max.y, tolerance));
}

void Builders::buildPolyLine(const Line& _line, const PolyLineOptions& _options, PolyLineOutput& _out) {
    
    // TODO: For outlines, we'll want another function which pre-processes the points into distinct line segments and then calls buildPolyLine as needed
    
    int lineSize = (int)_line.size();
    
    if (lineSize < 2) {
        return;
    }
    
    // TODO: pre-allocate output vectors; try estimating worst-case space usage
    
    glm::vec3 coordPrev, coordCurr, coordNext;
    glm::vec2 normPrev, normNext, miterVec;

    int cornersOnCap = (int)_options.cap;
    int trianglesOnJoin = (int)_options.join;
    
    // Process first point in line with an end cap
    coordCurr = _line[0];
    coordNext = _line[1];
    normNext = glm::normalize(perp2d(coordCurr, coordNext));
    addCap(coordCurr, normNext, cornersOnCap, true, _options.halfWidth, _out);
    addPolyLineVertex(coordCurr, normNext, {1.0f, 0.0f}, _options.halfWidth, _out);
    addPolyLineVertex(coordCurr, -normNext, {0.0f, 0.0f}, _options.halfWidth, _out);
    
    // Process intermediate points
    for (int i = 1; i < lineSize - 1; i++) {

        coordPrev = coordCurr;
        coordCurr = coordNext;
        coordNext = _line[i + 1];
        
        normPrev = normNext;
        normNext = glm::normalize(perp2d(coordCurr, coordNext));

        // Compute "normal" for miter joint
        miterVec = normPrev + normNext;
        float scale = sqrtf(2.0f / (1.0f + glm::dot(normPrev, normNext)) / glm::dot(miterVec, miterVec) );
        miterVec *= scale;
        
        float v = i / (float)lineSize;
        
        if (trianglesOnJoin == 0) {
            // Join type is a simple miter
            
            addPolyLineVertex(coordCurr, miterVec, {1.0, v}, _options.halfWidth, _out);
            addPolyLineVertex(coordCurr, -miterVec, {0.0, v}, _options.halfWidth, _out);
            indexPairs(1, _out.points.size(), _out.indices);
            
        } else {
            // Join type uses a fan of triangles
            
            bool isRightTurn = (normNext.x * normPrev.y - normNext.y * normPrev.x) > 0; // z component of cross(normNext, normPrev)
            
            if (isRightTurn) {
                
                addPolyLineVertex(coordCurr, miterVec, {1.0f, v}, _options.halfWidth, _out);
                addPolyLineVertex(coordCurr, -normPrev, {0.0f, v}, _options.halfWidth, _out);
                indexPairs(1, _out.points.size(), _out.indices);
                
                addFan(coordCurr, -normPrev, -normNext, {0.0f, v}, trianglesOnJoin, _options.halfWidth, _out);
                
                addPolyLineVertex(coordCurr, miterVec, {1.0f, v}, _options.halfWidth, _out);
                addPolyLineVertex(coordCurr, -normNext, {0.0f, v}, _options.halfWidth, _out);
                indexPairs(1, _out.points.size(), _out.indices);
                
            } else {
                
                addPolyLineVertex(coordCurr, normPrev, {1.0f, v}, _options.halfWidth, _out);
                addPolyLineVertex(coordCurr, -miterVec, {0.0f, v}, _options.halfWidth, _out);
                indexPairs(1, _out.points.size(), _out.indices);
                
                addFan(coordCurr, normPrev, normNext, {0.0f, v}, trianglesOnJoin, _options.halfWidth, _out);
                
                addPolyLineVertex(coordCurr, normNext, {1.0f, v}, _options.halfWidth, _out);
                addPolyLineVertex(coordCurr, -miterVec, {0.0f, v}, _options.halfWidth, _out);
                indexPairs(1, _out.points.size(), _out.indices);
                
            }
            
        }
    }
    
    // Process last point in line with a cap
    addPolyLineVertex(coordNext, normNext, {1.0f, 1.0f}, _options.halfWidth, _out);
    addPolyLineVertex(coordNext, -normNext, {0.0f, 1.0f}, _options.halfWidth, _out);
    indexPairs(1, _out.points.size(), _out.indices);
    addCap(coordNext, normNext, cornersOnCap , false, _options.halfWidth, _out);
    
}

void Builders::buildQuadAtPoint(const Point& _point, const glm::vec3& _normal, float halfWidth, float height, PolygonOutput& _out) {

}
