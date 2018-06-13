#include <iostream>
#include "maya_includes.h"
#include "CircBuffer.h"
#include "Mutex.h"
#include <vector>
#include <queue>
#define BUFFERSIZE 8<<20

using namespace std;

MCallbackIdArray idList;
size_t localHead = 0;


CircBuffer producer = CircBuffer(L"Buffer", BUFFERSIZE, true, 256);

Mutex mtx;

void transformProducer();
void meshProducer();
void transformFn(MObject &node);
void meshFn(MObject &node);
void mainHeaderPrinter();
void meshHeaderPrinter();

struct mMainHeader
{
	unsigned int transform = 0;
	unsigned int mesh = 0;
};

struct mTransform
{
	string name;
	float translateX, translateY, translateZ,
		rotateX, rotateY, rotateZ,
		scaleX, scaleY, scaleZ;
};

struct mVertex
{
	float posX, posY, posZ;
	float norX, norY, norZ;
	float U, V;
};

struct mMesh
{
	//namn och sånt
	string name;
	int materialId;

};
vector<mVertex>vertices;
struct mMeshHeader
{
	unsigned int verticesCount; // samma för normals och uvs

};

//vector<meshHeader>meshList;
//vector<transformHeader>transformList;

//struct mainHeader
//{
//	unsigned int meshCount;
//	unsigned int transformList;
//}mainH;

std::queue<MObject> queueList;
//std::queue<mVertex> vertexList;
std::queue<mMesh> meshVertexList;
std::queue<mMeshHeader> meshList;
std::queue<mMainHeader> mainList;
std::queue<mTransform> transformList;
char* msg = new char[(BUFFERSIZE / 4)];

void nodeNameChangeFn(MObject &node, const MString &str, void *clientData)
{
	MString typeStr = node.apiTypeStr();
	if (node.hasFn(MFn::kTransform))
	{
		MGlobal::displayInfo(typeStr + " has been renamed. Previous name: " + str);
	}
	if (node.hasFn(MFn::kMesh))
	{
		MGlobal::displayInfo(typeStr + " has been renamed. Previous name: " + str);
	}
}

void attributeChangedFn(MNodeMessage::AttributeMessage attrMsg, MPlug &plug, MPlug &otherPlug, void *clientData)
{
	MStatus res;

	if (attrMsg & MNodeMessage::AttributeMessage::kAttributeSet && !plug.isArray() && plug.isElement())
	{
		//MPlug pluggy = plug;
		//bool yesplx;
		//pluggy.findPlug(pnts, yesplx, result);

		MGlobal::displayInfo("Attribute Changed: " + plug.name());
		MFnMesh test(plug.node(), &res);
		MPoint pointy;
		test.getPoint(plug.logicalIndex(), pointy);

		// rad, siffra, logical eller physical index. node maya pnts[5]
	} // alla plugs hör till en node.   om man tar plug.node har man meshen
	  //MGlobal::displayInfo("A transformation node in the Scene has changed: " + plug.name());

}

void nodeDirtyFn(MObject &node, void *clientData)
{
	MGlobal::displayInfo("you dirty node...");
}
void queueFn(MObject &node)
{
	if (node.hasFn(MFn::kTransform))
	{
		MGlobal::displayInfo("QueueList kTransform: " + queueList.size());
		transformFn(node);
	}
	if (node.hasFn(MFn::kMesh))
	{
		MGlobal::displayInfo("QueueList kMesh: " + queueList.size());
		meshFn(node);
	}
}
void timerFn(float elapsedTime, float lastTime, void *clientData)
{

	//MGlobal::displayInfo(MString("Time is: ") + elapsedTime);

	if (queueList.size() != 0)
	{
		MGlobal::displayInfo(MString("QueueList Size: ") + queueList.size());
		MGlobal::displayInfo(MString("QueueList Next: ") + queueList.front().apiTypeStr());
		queueFn(queueList.front());
	}

}
void transformFn(MObject &node)
{
	MStatus result;

	MString typeStr = node.apiTypeStr();
	MGlobal::displayInfo("A node has been created in the Scene of following type: " + typeStr);
	MFnTransform transform(node, &result);
	if (result == MStatus::kSuccess)
	{
		mTransform th;
		MString name = transform.name();
		MVector translate = transform.getTranslation(MSpace::kWorld, &result);
		double scale[3];
		transform.getScale(scale);
		double rotation[3];
		MTransformationMatrix::RotationOrder ro;
		transform.getRotation(rotation, ro);
		MGlobal::displayInfo("Transform Name: " + name);
		MGlobal::displayInfo(MString("Transform Translate: ") + translate.x + ", " + translate.y + ", " + translate.z);
		MGlobal::displayInfo(MString("Transform Rotate: ") + rotation[0] + ", " + rotation[1] + ", " + rotation[2]);
		MGlobal::displayInfo(MString("Transform Scale: ") + scale[0] + ", " + scale[1] + ", " + scale[2]);

		th.name = name.asChar();
		th.translateX = translate.x;
		th.translateY = translate.y;
		th.translateZ = translate.z;

		th.rotateX = rotation[0];
		th.rotateY = rotation[1];
		th.rotateZ = rotation[2];

		th.scaleX = scale[0];
		th.scaleY = scale[1];
		th.scaleZ = scale[2];

		transformList.push(th);
		transformProducer(); //new, added 2018-06-12
	}
	else
		MGlobal::displayInfo("Creation of a transform resulted other then success");
}

void meshFn(MObject &node)
{
	MString typeStr = node.apiTypeStr();
	MStatus res;
	MFnMesh mesh(node, &res);

	if (res == MStatus::kSuccess)
	{
		MGlobal::displayInfo("A node has been created in the Scene of following type: " + typeStr);
		mMainHeader mMain;
		mMesh myMesh;
		mMeshHeader mh;
		MString name = mesh.name();
		MGlobal::displayInfo(MString("Mesh Name: ") + name);

		MIntArray vertexCount; // Point count per POLYFACE  || 24
		MIntArray Polyface; // Point indices per POLYFACE are returned. || 24a tal mellan 1-8, vilka POLYFACE points sitter ihop med varandra? front face hörn point sitter ihop med top face ena hörn point tex.
		mesh.getVertices(vertexCount, Polyface);

		MIntArray vertexCounts; // The number of triangles for each polygon face || 36 
		MIntArray triangleIndices; // The index array for each triangle in face vertex space || 36 tal mellan 1-24, vissa points i POLYFACET delas av två TRIANGLAR.
		mesh.getTriangleOffsets(vertexCounts, triangleIndices);

		MFloatPointArray vPos;
		mesh.getPoints(vPos, MSpace::kObject);

		MIntArray normalCounts; //Number of normals for each face
		MIntArray normalIDs; //Storage for the per-polygon normal ids

		mesh.getNormalIds(normalCounts, normalIDs);

		MFloatVectorArray vNor;
		mesh.getNormals(vNor);

		MIntArray uvCounts; //The container for the uv counts for each polygon in the mesh
		MIntArray uvIds; //The container for the uv indices mapped to each polygon-vertex
		mesh.getAssignedUVs(uvCounts, uvIds);

		MFloatArray uArray;
		MFloatArray vArray;
		mesh.getUVs(uArray, vArray);

		mVertex vertex;
		for (unsigned int i = 0; i < triangleIndices.length(); i++)
		{
			vertex.posX = vPos[Polyface[triangleIndices[i]]].x;
			vertex.posY = vPos[Polyface[triangleIndices[i]]].y;
			vertex.posZ = vPos[Polyface[triangleIndices[i]]].z;

			MGlobal::displayInfo(MString("vertex position: ") + vPos[Polyface[triangleIndices[i]]].x + ", " + vPos[Polyface[triangleIndices[i]]].y + ", " + vPos[Polyface[triangleIndices[i]]].z + " in triangle references as vertexIndex: " + triangleIndices[i] + " on polyface references as pointIndex: " + Polyface[triangleIndices[i]] + " index in my own vertex array: " + i);

			vertex.norX = vNor[normalIDs[triangleIndices[i]]].x;
			vertex.norY = vNor[normalIDs[triangleIndices[i]]].y;
			vertex.norZ = vNor[normalIDs[triangleIndices[i]]].z;

			vertex.U = uArray[uvIds[triangleIndices[i]]];
			vertex.V = vArray[uvIds[triangleIndices[i]]];
			
			//vertexList.push(vertex);
			vertices.push_back(vertex);
		}

		mMain.mesh += 1;
		mainList.push(mMain);
		//mh.verticesCount = vertexList.size();
		mh.verticesCount = vertices.size();
		meshList.push(mh);
		meshVertexList.push(myMesh);
		mainHeaderPrinter();
		meshProducer();
		queueList.pop();
		MGlobal::displayInfo(MString("meshFn: Success"));
	}

	//MFn::Type test = node.apiType();
	//if !success add node to timerelapsed(node); that while(!canBeWritten())
	if (res != MStatus::kSuccess)
	{
		MGlobal::displayInfo(MString("meshFn: Fail, added to queueList")); //  because the mesh does not exist yet in the scene
		queueList.push(node);
	}
}//

void OnNodeAddFn(MObject &node, void *clientData)
{
	MStatus result;

	MString nodeType = node.apiTypeStr();
	MGlobal::displayInfo("*OnNodeAddFn* A Node Type Appeared: " + nodeType);

	if (node.hasFn(MFn::kTransform))
	{
		transformFn(node);
	}
	if (node.hasFn(MFn::kMesh))
	{
		meshFn(node);
	}
}

void OnNodeRemoveFn(MObject &node, void *clientData)
{
	if (node.hasFn(MFn::kTransform))
	{
		MGlobal::displayInfo(MString("Transform got removed! ") + MString(node.apiTypeStr()));
	}

}


void callbacksFn()
{
	MStatus result = MS::kSuccess;
	MCallbackId tempCallbackId;

	tempCallbackId = MDGMessage::addNodeAddedCallback(OnNodeAddFn, kDefaultNodeType, NULL, &result);

	if (result == MStatus::kSuccess)
	{
		MGlobal::displayInfo("addNodeAddedCallback: Success");
		idList.append(tempCallbackId);
	}
	else
	{
		MGlobal::displayInfo("addNodeAddedCallback: Fail");
	}

	tempCallbackId = MDGMessage::addNodeRemovedCallback(OnNodeRemoveFn, kDefaultNodeType, NULL, &result);

	if (result == MStatus::kSuccess)
	{
		MGlobal::displayInfo("addNodeRemovedCallback: Success");
		idList.append(tempCallbackId);
	}
	else
	{
		MGlobal::displayInfo("addNodeRemovedCallback: Fail");
	}

	tempCallbackId = MTimerMessage::addTimerCallback(2, timerFn, NULL, &result); // 0.033 update time, har för mig att den var bra

	if (result == MStatus::kSuccess)
	{
		MGlobal::displayInfo("addTimerCallback: Success");
		idList.append(tempCallbackId);
	}
	else
	{
		MGlobal::displayInfo("addTimerCallback: Fail");
	}

	tempCallbackId = MNodeMessage::addNameChangedCallback(MObject::kNullObj, nodeNameChangeFn, NULL, &result);

	if (result == MStatus::kSuccess)
	{
		MGlobal::displayInfo("addNameChangedCallback: Success");
		idList.append(tempCallbackId);
	}
	else
	{
		MGlobal::displayInfo("addNameChangedCallback: Fail");
	}

	tempCallbackId = MNodeMessage::addAttributeChangedCallback(MObject::kNullObj, attributeChangedFn, NULL, &result);

	if (result == MStatus::kSuccess)
	{
		MGlobal::displayInfo("addAttributeChangedCallback: Success");
		idList.append(tempCallbackId);
	}
	else
	{
		MGlobal::displayInfo("addAttributeChangedCallback: Fail");
	}

	tempCallbackId = MNodeMessage::addNodeDirtyCallback(MObject::kNullObj, nodeDirtyFn, NULL, &result);

	if (result == MStatus::kSuccess)
	{
		MGlobal::displayInfo("addNodeDirtyCallback: Success");
		idList.append(tempCallbackId);
	}
	else
	{
		MGlobal::displayInfo("addNodeDirtyCallback: Fail");
	}
}

EXPORT MStatus initializePlugin(MObject obj)
{
	MStatus result = MS::kSuccess;

	MFnPlugin myPlugin(obj, "Maya plugin", "1.0", "Any", &result);
	if (MFAIL(result))
	{
		CHECK_MSTATUS(result);
	}

	callbacksFn();

	MGlobal::displayInfo("<<			Maya plugin loaded!			>>");

	return result;
}

EXPORT MStatus uninitializePlugin(MObject obj)
{
	MFnPlugin plugin(obj);

	MGlobal::displayInfo("Maya plugin unloaded!");

	MMessage::removeCallbacks(idList);

	return MS::kSuccess;
}

void mainHeaderPrinter()
{

	MGlobal::displayInfo(MString("<Main Header>"));
	MGlobal::displayInfo(MString("{"));
	MGlobal::displayInfo(MString("Transforms: ") + transformList.size());
	MGlobal::displayInfo(MString("Meshes: ") + meshVertexList.size());
	MGlobal::displayInfo(MString("}"));
	meshHeaderPrinter();
}
void meshHeaderPrinter()
{
	MGlobal::displayInfo(MString("<Mesh Header>"));
	MGlobal::displayInfo(MString("{"));
	MGlobal::displayInfo(MString("Vertices: ") + vertices.size());
	MGlobal::displayInfo(MString("}"));
}

void transformMsgCreator(char* msg)
{
	mtx.lock();
	memcpy(msg + localHead,			//destination
		&transformList.front(),		//source
		sizeof(mTransform));		//size

	transformList.pop();
	localHead += sizeof(mTransform);
	mtx.unlock();
}

void meshMsgCreator(char* msg)
{
	mtx.lock();

	memcpy(msg + localHead,			// destination
		&mainList.front(),			// source  mainHeader.mesh==1
		sizeof(mMainHeader));		// size


	mainList.pop();
	localHead += sizeof(mMainHeader);


	memcpy(msg + localHead,			// destination
		&meshList.front(),			// source  meshHeader.vertexCount
		sizeof(mMeshHeader));		// size

	meshList.pop();
	localHead += sizeof(mMeshHeader);
	
	//Up til this point, the project works

	memcpy(msg + localHead,					// destination
		vertices.data(),						//  
		(vertices.size() * sizeof(mVertex)));	// size

	meshVertexList.pop();
	localHead += vertices.size() * sizeof(mVertex);

	mtx.unlock();

}

void transformProducer()
{
	transformMsgCreator(msg);

	producer.push(msg, localHead);
}

void meshProducer()
{
	meshMsgCreator(msg);

	//producer.push(msg, localHead);
}