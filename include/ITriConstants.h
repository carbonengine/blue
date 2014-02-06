/* 
	*************************************************************************

	ITriConstants.h

	Author:    Hilmar Veigar Pétursson
	Created:   August 2001
	OS:        Win32
	Project:   Trinity

	Description:   

		Yeap


	Dependencies:

		DirectX 9.0, Probably more, ytbd.

	(c) CCP 2000

	*************************************************************************
*/

#pragma once

#ifndef _ITriConstants_H_
#define _ITriConstants_H_


////////////////////////////////////////////////////////////////////////////
// ParticleSystem
////////////////////////////////////////////////////////////////////////////

enum TRIPARTICLESTATUS
{
	TRTPS_NEW = 0,
	TRTPS_LIVE = 1,
	TRTPS_DEAD = 2,
	TRTPS_DOOMED = 3
};

enum TRIPARTICLETYPE
{
	TRTPT_SIMPLE = 0,
	TRTPT_MOVING = 1,
	TRTPT_RIBBON = 2
};

enum TRIPARTICLEBIRHT
{
	TRTPB_POINT = 0,
	TRTPB_FIELD = 1
};

const int TRTPD_NONE       = 0x0000;
const int TRTPD_TIME       = 0x0001;
const int TRTPD_RANGE      = 0x0002;
const int TRTPD_CAMERADIST = 0x0004;
const int TRTPD_NEEDED     = 0x0008;


const int TRTPA_NONE       = 1;
const int TRTPA_4          = 4;
const int TRTPA_16         = 16;
const int TRTPA_64         = 64;
const int TRTPA_128         = 128;
const int TRTPA_256         = 256;
const int TRTPA_512         = 512;

enum TRIPARTICLECYCLE
{
	TRTPC_NONE = 0,
	TRTPC_RANDOM_LOOP = 1,
	TRTPC_RANDOM_HOLD = 2,
	TRTPC_CONSTANT = 3,
	TRTPC_LIFETIME = 4
};

////////////////////////////////////////////////////////////////////////////
// Boid Swarm constants
////////////////////////////////////////////////////////////////////////////

enum TRIBOIDSWARMTYPE
{
	TRTBST_WHIP = 0,
	TRTBST_TRAIL = 1
};

////////////////////////////////////////////////////////////////////////////
// Material sources and passes
////////////////////////////////////////////////////////////////////////////

const int D3DBLENDOP_DISABLE = 0; // this op is sort of "missing" from D3D

enum TRIMATSOURCE { 
	TRIMATSRC_NONE = 0,
	TRIMATSRC_SHADER = 1,
	TRIMATSRC_AREA = 2,
	TRIMATSRC_SCENE = 3,
	TRIMATSRC_VERTEX = 4
};

const int MAX_TEXTURESTAGES = 4;


////////////////////////////////////////////////////////////////////////////
// Curve constants
////////////////////////////////////////////////////////////////////////////

enum TRIEXTRAPOLATION { 
	TRIEXT_NONE = 0,
	TRIEXT_CONSTANT = 1,
	TRIEXT_GRADIENT = 2,
	TRIEXT_CYCLE = 3,
};

enum TRIINTERPOLATION { 
	TRIINT_NONE = 0,
	TRIINT_CONSTANT = 1,
	TRIINT_LINEAR = 2,
	TRIINT_HERMITE = 3,
	TRIINT_CATMULLROM = 4,
	TRIINT_SLERP = 5,
	TRIINT_SQUAD = 6,
	TRIINT_SIGMOID = 7
};


////////////////////////////////////////////////////////////////////////////
// Shader constants
////////////////////////////////////////////////////////////////////////////

#define TRIRS_OPAQUE            0x00000001  // render shaders that are opaque
#define TRIRS_TRANSPARENT       0x00000002  // render shaders that are transparent
#define TRIRS_SUBMIT_TRANS		0x00000004  // submit transparent objects for later rendering
#define	TRIRS_ALL   TRIRS_TRANSPARENT | TRIRS_OPAQUE

enum TRISTAGESELECTION { 
	TRISTS_USE2STAGEPASSES = 2,
	TRISTS_USE3STAGEPASSES = 3,
	TRISTS_USE4STAGEPASSES = 4
};


enum TRITEXSOURCE { 
	TRITEXSRC_NONE = 0,
	TRITEXSRC_SHADER = 1,
	TRITEXSRC_AREA = 2,
	TRITEXSRC_SCENE = 3
};

////////////////////////////////////////////////////////////////////////////
// Texture stage constants
////////////////////////////////////////////////////////////////////////////

#define D3DTA_CUSTOMCOLOR 0x20000001

#define D3DTA_VIEWVEC 0x10000001
#define D3DTA_OBJECTVEC 0x1000000F
#define D3DTA_LERPVIEWVEC_BYDIFFUSEALPHA 0x18000001
#define D3DTA_SUNVEC 0x10000002
#define D3DTA_SUNDOTVIEWVEC 0x10000004
#define D3DTA_VOLUME 0x1F000001
#define D3DTA_CONTRASTVIEW_BYALPHA 0x10000008


#define D3DTA_SCENE0 0x11000000
#define D3DTA_SCENE1 0x11000001


////////////////////////////////////////////////////////////////////////////
// Transform constants
////////////////////////////////////////////////////////////////////////////

// large used for picking
const float HUGE_NUMBER = 1.0e+36F;

// smallest such that 1.0+FLT_EPSILON != 1.0 
const float TRI_FLT_EPSILON = 1.192092896e-07F;
const float TRI_FLT_MAX     = 3.402823466e+38F;

const long TRIRO_FRONT_TO_BACK = 0;
const long TRIRO_BACK_TO_FRONT = 1;  

enum TRITRANSFORMBASE{ 
	TRITB_OBJECT = 0,
	TRITB_CAMERA_ROTATION = 1,
	TRITB_CAMERA_TRANSLATION = 2,
	TRITB_CAMERA = 3,
	TRITB_CAMERA_ROTATION_ALIGNED = 4,
	TRITB_FIXED = 5,
	TRITB_CAMERA_ROTATION_FALLOFF = 6,
	TRITB_CAMERA_ROTATION_ALIGNED_SYMMETRY = 7,
	TRITB_CAMERA_ROTATION_FALLOFF_SYMMETRY = 8,
	TRITB_BOOSTER = 9,
	TRITB_SIMPLE_HALO = 10,
	TRITB_SIMPLE_HALO_SYMMETRY = 11,
	TRITB_BOOSTER_FALLOFF = 12,
	TRITB_WORLD = 13,
	TRITB_SIMPLE_HALO_FALLOFF = 14,
	TRITB_SIMPLE_SPRITE = 15,
	TRITB_SIMPLE_SPRITE_FALLOFF = 16,
	TRITB_SIMPLE_SPRITE_CONSTANT = 17,
};

enum TRITRANSFORMAXIS{ 
	TRITA_X = 0,
	TRITA_Y = 1,
	TRITA_Z = 2
};

enum TRIOPERATOR{ 
	TRIOP_MULTIPLY = 0,
	TRIOP_ADD = 1,
};

const long TRITBF_SCALING      = 0x00000001;
const long TRITBF_TRANSLATION  = 0x00000002;
const long TRITBF_ROTATION_FWD = 0x00000004;
const long TRITBF_ROTATION_UP  = 0x00000008;




////////////////////////////////////////////////////////////////////////////
// LOD constants
////////////////////////////////////////////////////////////////////////////

const int TRILOD_MAXLODS = 8;

enum TRICLOUDTYPE{ 
	TRICT_SPRITE = 0,
	TRICT_POINT = 1,
	TRICT_LINE = 2,
};


////////////////////////////////////////////////////////////////////////////
// Vertex Format constants
////////////////////////////////////////////////////////////////////////////

#define D3DVSDT_NONE 0xFFFFFFFF

////////////////////////////////////////////////////////////////////////////
// Cloud Type constants
////////////////////////////////////////////////////////////////////////////

enum TRILODBY{ 
	TRILB_NONE = 0,
	TRILB_CAMERA_DISTANCE = 1,
	TRITB_CAMERA_DISTANCE_FOV_HEIGHT = 2,
};

////////////////////////////////////////////////////////////////////////////
// Clipping / scissoring for UI
////////////////////////////////////////////////////////////////////////////
enum SCISSORMODE {
		SCISSOR_NONE = 0,
		SCISSOR_SCISSOR = 1,
		SCISSOR_EMULATE = 2,
		SCISSOR_CHOOSE = -1,
};

////////////////////////////////////////////////////////////////////////////
// TriPoseClipTime
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
// Clipping / scissoring for UI
////////////////////////////////////////////////////////////////////////////
enum POSECLIPTIME {
		TRIPC_WORLD= 0,
		TRIPC_LOCAL= 1,
		TRIPC_OFFSETX= 2,
		TRIPC_OFFSETY= 3,
		TRIPC_OFFSETZ= 4,
		TRIPC_YAW= 5,
};

////////////////////////////////////////////////////////////////////////////
// TriBipedState
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
// Clipping / scissoring for UI
////////////////////////////////////////////////////////////////////////////
enum BIPEDSTATE {
		TRIBPS_IDLE= 0,
		TRIBPS_WALKING= 1,
		TRIBPS_TURNING= 2,
		TRIBPS_RUNNING= 3,
		TRIBPS_STRAFING= 4,
};

enum SKELETONTYPE {
		TRIST_MAIN = 0,
		TRIST_CLOTH_UPPER = 1,
		TRIST_CLOTH_LOWER = 2,
};

#endif