/******************************************************************************/
/*!
\file		GameState_Asteroids.cpp
\author 	DigiPen
\par    	email: digipen\@digipen.edu
\date   	February 01, 20xx
\brief

Copyright (C) 20xx DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
/******************************************************************************/

#include "main.h"
#include "Collision.h"
#include "ResourceManager.h"
#include <string>
#include <fstream>
#include <iostream>

/******************************************************************************/
/*!
	Defines
*/
/******************************************************************************/
const unsigned int	GAME_OBJ_NUM_MAX		= 32;	//The total number of different objects (Shapes)
const unsigned int	GAME_OBJ_INST_NUM_MAX	= 2048;	//The total number of different game object instances

//Gameplay related variables and values
const float			GRAVITY					= -2.0f;
const float			JUMP_VELOCITY			= 11.0f;
const float			MOVE_VELOCITY_HERO		= 4.0f;
const float			MOVE_VELOCITY_ENEMY		= 7.5f;
const double		ENEMY_IDLE_TIME			= 2.0;
const int			HERO_LIVES				= 3;

//Flags
const unsigned int	FLAG_ACTIVE				= 0x00000001;
const unsigned int	FLAG_VISIBLE			= 0x00000002;
const unsigned int	FLAG_NON_COLLIDABLE		= 0x00000004;

//Collision flags
const unsigned int	COLLISION_LEFT			= 0x00000001;	//0001
const unsigned int	COLLISION_RIGHT			= 0x00000002;	//0010
const unsigned int	COLLISION_TOP			= 0x00000004;	//0100
const unsigned int	COLLISION_BOTTOM		= 0x00000008;	//1000


enum TYPE_OBJECT
{
	TYPE_OBJECT_EMPTY,			//0
	TYPE_OBJECT_COLLISION,		//1
	TYPE_OBJECT_HERO,			//2
	TYPE_OBJECT_ENEMY1,			//3
	TYPE_OBJECT_COIN			//4
};

//State machine states
enum STATE
{
	STATE_NONE,
	STATE_GOING_LEFT,
	STATE_GOING_RIGHT
};

//State machine inner states
enum INNER_STATE
{
	INNER_STATE_ON_ENTER,
	INNER_STATE_ON_UPDATE,
	INNER_STATE_ON_EXIT
};

/******************************************************************************/
/*!
	Struct/Class Definitions
*/
/******************************************************************************/
struct GameObj
{
	unsigned int		type;		// object type
	AEGfxVertexList *	pMesh;		// pbject
};


struct GameObjInst
{
	GameObj *		pObject;	// pointer to the 'original'
	unsigned int	flag;		// bit flag or-ed together
	float			scale;
	AEVec2			posCurr;	// object current position
	AEVec2			velCurr;	// object current velocity
	float			dirCurr;	// object current direction

	AEMtx33			transform;	// object drawing matrix
	
	AABB			boundingBox;// object bouding box that encapsulates the object

	//Used to hold the current 
	int				gridCollisionFlag;

	// pointer to custom data specific for each object type
	void*			pUserData;

	//State of the object instance
	enum			STATE state;
	enum			INNER_STATE innerState;

	//General purpose counter (This variable will be used for the enemy state machine)
	double			counter;

	// EXTRA CREDIT THINGS
	Sprite*			_sprite{ nullptr };
};


/******************************************************************************/
/*!
	File globals
*/
/******************************************************************************/
static int				HeroLives;
static int				Hero_Initial_X;
static int				Hero_Initial_Y;
static int				TotalCoins;

// list of original objects
static GameObj			*sGameObjList;
static unsigned int		sGameObjNum;

// list of object instances
static GameObjInst		*sGameObjInstList;
static unsigned int		sGameObjInstNum;

//Binary map data
static int				**MapData;
static int				**BinaryCollisionArray;
static int				BINARY_MAP_WIDTH;
static int				BINARY_MAP_HEIGHT;
static GameObjInst		*pBlackInstance;
static GameObjInst		*pWhiteInstance;
static AEMtx33			MapTransform;

int						GetCellValue(int X, int Y);
int						CheckInstanceBinaryMapCollision(float PosX, float PosY, 
														float scaleX, float scaleY);
void					SnapToCell(float *Coordinate);
int						ImportMapDataFromFile(char *FileName);
void					FreeMapData(void);

// function to create/destroy a game object instance
static GameObjInst*		gameObjInstCreate (unsigned int type, float scale, 
											AEVec2* pPos, AEVec2* pVel, 
											float dir, enum STATE startState);
static void				gameObjInstDestroy(GameObjInst* pInst);

//We need a pointer to the hero's instance for input purposes
static GameObjInst		*pHero;

//State machine functions
void					EnemyStateMachine(GameObjInst *pInst);

//my variables
bool					isLevelTwo = true;
bool					_extra_credit = false;
float					cameraX = 0.0f;
float					cameraY = 0.0f;
float					worldScaleX = 50.0f;
float					worldScaleY = 50.0f;
static int				**CellSpriteData;

// for my sanity using aevec2
AEVec2& operator+=(AEVec2& lhs, const AEVec2& rhs) {
	AEVec2Add(&lhs, &lhs, const_cast<AEVec2*>(&rhs));
	return lhs;
}

AEVec2 operator+(const AEVec2& lhs, const AEVec2& rhs) {
	return { lhs.x + rhs.x,lhs.y + rhs.y };
}

AEVec2 operator*(const AEVec2& lhs, const float& rhs) {
	return { lhs.x * rhs,lhs.y * rhs };
}

AEVec2 operator-(const AEVec2& vec) {
	return { -vec.x,-vec.y };
}

/******************************************************************************/
/*!

*/
/******************************************************************************/
void GameStatePlatformLoad(void)
{
	sGameObjList = (GameObj *)calloc(GAME_OBJ_NUM_MAX, sizeof(GameObj));
	sGameObjInstList = (GameObjInst *)calloc(GAME_OBJ_INST_NUM_MAX, sizeof(GameObjInst));
	sGameObjNum = 0;


	GameObj* pObj;

	//Creating the black object
	pObj		= sGameObjList + sGameObjNum++;
	if (!pObj) {
		return;
	}
	pObj->type	= TYPE_OBJECT_EMPTY;


	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFF000000, 0.0f, 0.0f,
		 0.5f,  -0.5f, 0xFF000000, 0.0f, 0.0f, 
		-0.5f,  0.5f, 0xFF000000, 0.0f, 0.0f);
	
	AEGfxTriAdd(
		-0.5f, 0.5f, 0xFF000000, 0.0f, 0.0f,
		 0.5f,  -0.5f, 0xFF000000, 0.0f, 0.0f, 
		0.5f,  0.5f, 0xFF000000, 0.0f, 0.0f);

	pObj->pMesh = AEGfxMeshEnd();
	AE_ASSERT_MESG(pObj->pMesh, "fail to create object!!");
		
	
	//Creating the white object
	pObj		= sGameObjList + sGameObjNum++;
	pObj->type	= TYPE_OBJECT_COLLISION;


	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFFFFFFFF, 0.0f, 0.0f,
		 0.5f,  -0.5f, 0xFFFFFFFF, 0.0f, 0.0f, 
		-0.5f,  0.5f, 0xFFFFFFFF, 0.0f, 0.0f);
	
	AEGfxTriAdd(
		-0.5f, 0.5f, 0xFFFFFFFF, 0.0f, 0.0f, 
		 0.5f,  -0.5f, 0xFFFFFFFF, 0.0f, 0.0f, 
		0.5f,  0.5f, 0xFFFFFFFF, 0.0f, 0.0f);

	pObj->pMesh = AEGfxMeshEnd();
	AE_ASSERT_MESG(pObj->pMesh, "fail to create object!!");


	//Creating the hero object
	pObj		= sGameObjList + sGameObjNum++;
	pObj->type	= TYPE_OBJECT_HERO;


	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFF0000FF, 0.0f, 0.0f, 
		 0.5f,  -0.5f, 0xFF0000FF, 0.0f, 0.0f, 
		-0.5f,  0.5f, 0xFF0000FF, 0.0f, 0.0f);
	
	AEGfxTriAdd(
		-0.5f, 0.5f, 0xFF0000FF, 0.0f, 0.0f,
		 0.5f,  -0.5f, 0xFF0000FF, 0.0f, 0.0f, 
		0.5f,  0.5f, 0xFF0000FF, 0.0f, 0.0f);

	pObj->pMesh = AEGfxMeshEnd();
	AE_ASSERT_MESG(pObj->pMesh, "fail to create object!!");


	//Creating the enemey1 object
	pObj		= sGameObjList + sGameObjNum++;
	pObj->type	= TYPE_OBJECT_ENEMY1;


	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFFFF0000, 0.0f, 0.0f, 
		 0.5f,  -0.5f, 0xFFFF0000, 0.0f, 0.0f, 
		-0.5f,  0.5f, 0xFFFF0000, 0.0f, 0.0f);
	
	AEGfxTriAdd(
		-0.5f, 0.5f, 0xFFFF0000, 0.0f, 0.0f, 
		 0.5f,  -0.5f, 0xFFFF0000, 0.0f, 0.0f, 
		0.5f,  0.5f, 0xFFFF0000, 0.0f, 0.0f);

	pObj->pMesh = AEGfxMeshEnd();
	AE_ASSERT_MESG(pObj->pMesh, "fail to create object!!");


	//Creating the Coin object
	pObj		= sGameObjList + sGameObjNum++;
	pObj->type	= TYPE_OBJECT_COIN;


	AEGfxMeshStart();
	//Creating the circle shape
	int Parts = 12;
	for(float i = 0; i < Parts; ++i)
	{
		AEGfxTriAdd(
		0.0f, 0.0f, 0xFFFFFF00, 0.0f, 0.0f, 
		cosf(i*2*PI/Parts)*0.5f,  sinf(i*2*PI/Parts)*0.5f, 0xFFFFFF00, 0.0f, 0.0f, 
		cosf((i+1)*2*PI/Parts)*0.5f,  sinf((i+1)*2*PI/Parts)*0.5f, 0xFFFFFF00, 0.0f, 0.0f);
	}

	pObj->pMesh = AEGfxMeshEnd();
	AE_ASSERT_MESG(pObj->pMesh, "fail to create object!!");

	//Setting intital binary map values
	MapData = 0;
	BinaryCollisionArray = 0;
	BINARY_MAP_WIDTH = 0;
	BINARY_MAP_HEIGHT = 0;

	//Importing Data
	std::string level_path = "../Resources/Levels/";
	std::string level_file = "Exported.txt";
	if (isLevelTwo) {
		level_file = "Exported2.txt";
		_extra_credit = true;
	}
	if (!ImportMapDataFromFile(const_cast<char*>((level_path+level_file).c_str())))
		gGameStateNext = GS_QUIT;


	//Computing the matrix which take a point out of the normalized coordinates system
	//of the binary map
	/***********
	Compute a transformation matrix and save it in "MapTransform".
	This transformation transforms any point from the normalized coordinates system of the binary map.
	Later on, when rendering each object instance, we should concatenate "MapTransform" with the
	object instance's own transformation matrix

	Compute a translation matrix (-Grid width/2, -Grid height/2) and save it in "trans"
	Compute a scaling matrix and save it in "scale")
	Concatenate scale and translate and save the result in "MapTransform"
	***********/
	AEMtx33 scale, trans;
	AEMtx33Trans(&trans, -(float)BINARY_MAP_WIDTH / 2.0f, -(float)BINARY_MAP_HEIGHT / 2.0f);
	AEMtx33Scale(&scale, worldScaleX, worldScaleY);
	AEMtx33Concat(&MapTransform, &scale, &trans);

	ResourceManager::Instance().InitializeResources();
}

/******************************************************************************/
/*!

*/
/******************************************************************************/
void GameStatePlatformInit(void)
{
	ResourceManager& rm = ResourceManager::Instance();

	int i{ -1 }, j{ -1 };
	UNREFERENCED_PARAMETER(j);

	pHero = 0;
	pBlackInstance = 0;
	pWhiteInstance = 0;
	TotalCoins = 0;

	//Create an object instance representing the black cell.
	//This object instance should not be visible. When rendering the grid cells, each time we have
	//a non collision cell, we position this instance in the correct location and then we render it
	pBlackInstance = gameObjInstCreate(TYPE_OBJECT_EMPTY, 1.0f, 0, 0, 0.0f, STATE_NONE);
	pBlackInstance->flag ^= FLAG_VISIBLE;
	pBlackInstance->flag |= FLAG_NON_COLLIDABLE;

	//Create an object instance representing the white cell.
	//This object instance should not be visible. When rendering the grid cells, each time we have
	//a collision cell, we position this instance in the correct location and then we render it
	pWhiteInstance = gameObjInstCreate(TYPE_OBJECT_COLLISION, 1.0f, 0, 0, 0.0f, STATE_NONE);
	pWhiteInstance->flag ^= FLAG_VISIBLE;
	pWhiteInstance->flag |= FLAG_NON_COLLIDABLE;

	//Setting the inital number of hero lives
	HeroLives = HERO_LIVES;

	GameObjInst* pInst{ nullptr };
	AEVec2 Pos{ 0 };

	UNREFERENCED_PARAMETER(pInst);
	UNREFERENCED_PARAMETER(Pos);

	// creating the main character, the enemies and the coins according 
	// to their initial positions in MapData

	/***********
	Loop through all the array elements of MapData 
	(which was initialized in the "GameStatePlatformLoad" function
	from the .txt file
		if the element represents a collidable or non collidable area
			don't do anything

		if the element represents the hero
			Create a hero instance
			Set its position depending on its array indices in MapData
			Save its array indices in Hero_Initial_X and Hero_Initial_Y 
			(Used when the hero dies and its position needs to be reset)

		if the element represents an enemy
			Create an enemy instance
			Set its position depending on its array indices in MapData
			
		if the element represents a coin
			Create a coin instance
			Set its position depending on its array indices in MapData
			
	***********/
	GameObjInst* inst{ nullptr };
	for (i = 0; i < BINARY_MAP_WIDTH; ++i) {
		for (j = 0; j < BINARY_MAP_HEIGHT; ++j)
		{
			if (MapData[i][j] == TYPE_OBJECT_EMPTY || MapData[i][j] == TYPE_OBJECT_COLLISION) {
				continue;
			}
			AEVec2 pos{ (float)i+0.5f,(float)j+0.5f };
			switch (MapData[i][j]) {
			case TYPE_OBJECT_HERO:
				pHero = gameObjInstCreate(MapData[i][j], 1.0f, &pos, nullptr, 0.0f, STATE::STATE_NONE);
				pHero->_sprite = &rm.LoadSprite("temp2.png", 3, 4, 10, 0.2f);
				Hero_Initial_X = i;
				Hero_Initial_Y = j;
				break;
			case TYPE_OBJECT_ENEMY1:
				inst = gameObjInstCreate(MapData[i][j], 1.0f, &pos, nullptr, 0.0f, STATE::STATE_GOING_RIGHT);
				inst->_sprite = &rm.LoadSprite("temp2.png", 3, 4, 10, 0.2f);
				break;
			case TYPE_OBJECT_COIN:
				inst = gameObjInstCreate(MapData[i][j], 1.0f, &pos, nullptr, 0.0f, STATE::STATE_NONE);
				inst->_sprite = &rm.LoadSprite("coin.png", 3, 3, 8, 0.1f);
				inst->_sprite->_scale_x = 1.5f;
				inst->_sprite->_scale_y = 1.5f;
				break;
			}
			/*if (MapData[i][j] == TYPE_OBJECT_HERO) {
				pHero = gameObjInstCreate(MapData[i][j], 1.0f, &pos, nullptr, 0.0f, STATE::STATE_NONE);
				Hero_Initial_X = i;
				Hero_Initial_Y = j;
			}
			else if (MapData[i][j] == TYPE_OBJECT_ENEMY1) {
				gameObjInstCreate(MapData[i][j], 1.0f, &pos, nullptr, 0.0f, STATE::STATE_GOING_RIGHT);
			}
			else {
				gameObjInstCreate(MapData[i][j], 1.0f, &pos, nullptr, 0.0f, STATE::STATE_NONE);
			}*/
		}
	}
}

/******************************************************************************/
/*!

*/
/******************************************************************************/
void GameStatePlatformUpdate(void)
{
	if (AEInputCheckTriggered('E')) {
		_extra_credit = !_extra_credit;
	}
	float _dt = (float)AEFrameRateControllerGetFrameTime();
	int i{ -1 }, j{ -1 };
	GameObjInst* pInst{ nullptr };

	UNREFERENCED_PARAMETER(j);
	UNREFERENCED_PARAMETER(pInst);

	// Camera code
	if (isLevelTwo && pHero) {
		AEMtx33 scale, trans, cam;
		AEMtx33Trans(&trans, -pHero->posCurr.x, -pHero->posCurr.y);
		AEMtx33Scale(&scale, worldScaleX, worldScaleY);
		AEMtx33Concat(&MapTransform, &scale, &trans);
	}
	//Handle Input
	/***********
	if right is pressed
		Set hero velocity X to MOVE_VELOCITY_HERO
	else
	if left is pressed
		Set hero velocity X to -MOVE_VELOCITY_HERO
	else
		Set hero velocity X to 0

	if space is pressed AND Hero is colliding from the bottom
		Set hero velocity Y to JUMP_VELOCITY

	if Escape is pressed
		Exit to menu
	***********/
	if (pHero) {
		if (AEInputCheckCurr(AEVK_RIGHT)) {
			pHero->velCurr.x = MOVE_VELOCITY_HERO;
		}
		else if (AEInputCheckCurr(AEVK_LEFT)) {
			pHero->velCurr.x = -MOVE_VELOCITY_HERO;
		}
		else {
			pHero->velCurr.x = 0.0f;
		}
		if ((pHero->gridCollisionFlag & COLLISION_BOTTOM) == COLLISION_BOTTOM && AEInputCheckCurr(AEVK_SPACE)) {
			pHero->velCurr.y = JUMP_VELOCITY;
		}
	}


	//Update object instances physics and behavior
	for(i = 0; i < GAME_OBJ_INST_NUM_MAX; ++i)
	{
		pInst = sGameObjInstList + i;

		// skip non-active object
		if (0 == (pInst->flag & FLAG_ACTIVE))
			continue;


		/****************
		Apply gravity
			Velocity Y = Gravity * Frame Time + Velocity Y

		If object instance is an enemy
			Apply enemy state machine
		****************/
		if (pInst->pObject->type == TYPE_OBJECT_COIN) {
			continue;
		}

		if (pInst->pObject->type == TYPE_OBJECT_ENEMY1) {
			EnemyStateMachine(pInst);
		}

		pInst->velCurr.y += GRAVITY * _dt;
	}
	AEVec2 BOUNDING_RECT_SIZE = { 0.5f,0.5f };
	//Update object instances positions
	for(i = 0; i < GAME_OBJ_INST_NUM_MAX; ++i)
	{
		pInst = sGameObjInstList + i;

		// skip non-active object
		if (0 == (pInst->flag & FLAG_ACTIVE))
			continue;

		/**********
		update the position using: P1 = V1*dt + P0
		Get the bouding rectangle of every active instance:
			boundingRect_min = -BOUNDING_RECT_SIZE * instance->scale + instance->pos
			boundingRect_max = BOUNDING_RECT_SIZE * instance->scale + instance->pos
		**********/
		pInst->posCurr += pInst->velCurr * _dt;
		
		if (pInst->_sprite) {
			pInst->_sprite->_x = pInst->posCurr.x;
			pInst->_sprite->_y = pInst->posCurr.y;
		}
		
		pInst->boundingBox.min = pInst->posCurr + -BOUNDING_RECT_SIZE * pInst->scale;
		pInst->boundingBox.max = pInst->posCurr + BOUNDING_RECT_SIZE * pInst->scale;
	}

	//Check for grid collision
	for(i = 0; i < GAME_OBJ_INST_NUM_MAX; ++i)
	{
		pInst = sGameObjInstList + i;

		// skip non-active object instances
		if (0 == (pInst->flag & FLAG_ACTIVE) || 0 == (pInst->flag & FLAG_VISIBLE))
			continue;

		/*************
		Update grid collision flag

		if collision from bottom
			Snap to cell on Y axis
			Velocity Y = 0

		if collision from top
			Snap to cell on Y axis
			Velocity Y = 0
	
		if collision from left
			Snap to cell on X axis
			Velocity X = 0

		if collision from right
			Snap to cell on X axis
			Velocity X = 0
		*************/
		pInst->gridCollisionFlag = CheckInstanceBinaryMapCollision(pInst->posCurr.x, pInst->posCurr.y, pInst->scale, pInst->scale);
		if (((pInst->gridCollisionFlag & COLLISION_LEFT) == COLLISION_LEFT) || ((pInst->gridCollisionFlag & COLLISION_RIGHT) == COLLISION_RIGHT)) {
			SnapToCell(&pInst->posCurr.x);
			pInst->velCurr.x = 0;
		}
		if (((pInst->gridCollisionFlag & COLLISION_TOP) == COLLISION_TOP) || ((pInst->gridCollisionFlag & COLLISION_BOTTOM) == COLLISION_BOTTOM)) {
			SnapToCell(&pInst->posCurr.y);
			pInst->velCurr.y = 0;
		}
	}


	//Checking for collision among object instances:
	//Hero against enemies
	//Hero against coins

	/**********
	for each game object instance
		Skip if it's inactive or if it's non collidable

		If it's an enemy
			If collision between the enemy instance and the hero (rectangle - rectangle)
				Decrement hero lives
				Reset the hero's position in case it has lives left, otherwise RESTART the level

		If it's a coin
			If collision between the coin instance and the hero (rectangle - rectangle)
				Remove the coin and decrement the coin counter.
				Quit the game level to the menu in case no more coins are left
	**********/
	
	for(i = 0; i < GAME_OBJ_INST_NUM_MAX; ++i)
	{
		pInst = sGameObjInstList + i;

		if (0 == (pInst->flag & FLAG_ACTIVE))
			continue;

		// with enemy
		if (pInst && pHero) {
			if (pInst->pObject->type == TYPE_OBJECT_ENEMY1) {
				if (CollisionIntersection_RectRect(pInst->boundingBox, pInst->velCurr, pHero->boundingBox, pHero->velCurr)) {
					--HeroLives;
					if (HeroLives <= 0) {
						gGameStateCurr = GS_RESTART;
					}
					else {
						pHero->posCurr.x = Hero_Initial_X + 0.5f;
						pHero->posCurr.y = Hero_Initial_Y + 0.5f;
					}
				}
			}
		}

		// with coin
		if (pInst && pHero) {
			if (pInst->pObject->type == TYPE_OBJECT_COIN) {
				if (CollisionIntersection_RectRect(pInst->boundingBox, pInst->velCurr, pHero->boundingBox, pHero->velCurr)) {
					pInst->flag &= ~FLAG_ACTIVE;
					pInst->_sprite->_active = false;
				}
			}
		}
	}

	
	//Computing the transformation matrices of the game object instances
	for(i = 0; i < GAME_OBJ_INST_NUM_MAX; ++i)
	{
		AEMtx33 scale, rot, trans;
		pInst = sGameObjInstList + i;

		// skip non-active object
		if (0 == (pInst->flag & FLAG_ACTIVE))
			continue;

		AEMtx33Scale(&scale, pInst->scale, pInst->scale);
		AEMtx33Rot(&rot, pInst->dirCurr);
		AEMtx33Trans(&trans, pInst->posCurr.x, pInst->posCurr.y);
		AEMtx33Concat(&rot, &rot, &scale);
		AEMtx33Concat(&pInst->transform, &trans, &rot);
	}
}

/******************************************************************************/
/*!

*/
/******************************************************************************/
void GameStatePlatformDraw(void)
{
	AEGfxSetRenderMode(AEGfxRenderMode::AE_GFX_RM_COLOR);
	//Drawing the tile map (the grid)
	int i, j;
	AEMtx33 cellTranslation{ 0 }, cellFinalTransformation{ 0 };

	UNREFERENCED_PARAMETER(cellTranslation);
	UNREFERENCED_PARAMETER(cellFinalTransformation);

	//Drawing the tile map

	/******REMINDER*****
	You need to concatenate MapTransform with the transformation matrix 
	of any object you want to draw. MapTransform transform the instance 
	from the normalized coordinates system of the binary map
	*******************/

	/*********
	for each array element in BinaryCollisionArray (2 loops)
		Compute the cell's translation matrix acoording to its 
		X and Y coordinates and save it in "cellTranslation"
		Concatenate MapTransform with the cell's transformation 
		and save the result in "cellFinalTransformation"
		Send the resultant matrix to the graphics manager using "AEGfxSetTransform"

		Draw the instance's shape depending on the cell's value using "AEGfxMeshDraw"
			Use the black instance in case the cell's value is TYPE_OBJECT_EMPTY
			Use the white instance in case the cell's value is TYPE_OBJECT_COLLISION
	*********/
	for (i = 0; i < BINARY_MAP_WIDTH; ++i)
		for (j = 0; j < BINARY_MAP_HEIGHT; ++j)
		{
			if (BinaryCollisionArray[i][j] == TYPE_OBJECT::TYPE_OBJECT_EMPTY) {
				AEMtx33Trans(&cellTranslation, i + 0.5f, j + 0.5f);
				AEMtx33Concat(&cellFinalTransformation, &MapTransform, &cellTranslation);
				AEGfxSetTransform(cellFinalTransformation.m);
				AEGfxMeshDraw(pBlackInstance->pObject->pMesh, AEGfxMeshDrawMode::AE_GFX_MDM_TRIANGLES);
			}
			if (BinaryCollisionArray[i][j] == TYPE_OBJECT::TYPE_OBJECT_COLLISION) {
				AEMtx33Trans(&cellTranslation, i + 0.5f, j + 0.5f);
				AEMtx33Concat(&cellFinalTransformation, &MapTransform, &cellTranslation);
				AEGfxSetTransform(cellFinalTransformation.m);
				AEGfxMeshDraw(pWhiteInstance->pObject->pMesh, AEGfxMeshDrawMode::AE_GFX_MDM_TRIANGLES);
			}
		}
	if (!_extra_credit) {

		//Drawing the object instances
		/**********
		For each active and visible object instance
			Concatenate MapTransform with its transformation matrix
			Send the resultant matrix to the graphics manager using "AEGfxSetTransform"
			Draw the instance's shape using "AEGfxMeshDraw"
		**********/
		for (i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
		{
			GameObjInst* pInst = sGameObjInstList + i;

			// skip non-active object
			if (0 == (pInst->flag & FLAG_ACTIVE) || 0 == (pInst->flag & FLAG_VISIBLE))
				continue;

			//Don't forget to concatenate the MapTransform matrix with the transformation of each game object instance
			AEMtx33Concat(&cellFinalTransformation, &MapTransform, &pInst->transform);
			AEGfxSetTransform(cellFinalTransformation.m);
			AEGfxMeshDraw(pInst->pObject->pMesh, AEGfxMeshDrawMode::AE_GFX_MDM_TRIANGLES);
		}
	}

	// Draw Extra Credit Resources
	if (_extra_credit) {
		ResourceManager::Instance().DrawSprites(g_dt, MapTransform);
	}

	//char strBuffer[100];
	//memset(strBuffer, 0, 100*sizeof(char));
	//sprintf_s(strBuffer, "Lives:  %i", HeroLives);
	////AEGfxPrint(650, 30, (u32)-1, strBuffer);	
	//printf("%s \n", strBuffer);
}

/******************************************************************************/
/*!

*/
/******************************************************************************/
void GameStatePlatformFree(void)
{
	// kill all object in the list
	for (unsigned int i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
		gameObjInstDestroy(sGameObjInstList + i);
	ResourceManager::Instance().FreeResources();
}

/******************************************************************************/
/*!

*/
/******************************************************************************/
void GameStatePlatformUnload(void)
{
	// free all CREATED mesh
	for (u32 i = 0; i < sGameObjNum; i++)
		AEGfxMeshFree(sGameObjList[i].pMesh);

	/*********
	Free the map data
	*********/
	FreeMapData();
	/*for (int i = 0; i < BINARY_MAP_WIDTH; ++i) {
		delete[] MapData[i];
		delete[] BinaryCollisionArray[i];
	}
	delete[] MapData;
	delete[] BinaryCollisionArray;*/
	// free resources
	ResourceManager::Instance().UnloadResources();
}

/******************************************************************************/
/*!

*/
/******************************************************************************/
GameObjInst* gameObjInstCreate(unsigned int type, float scale, 
							   AEVec2* pPos, AEVec2* pVel, 
							   float dir, enum STATE startState)
{
	AEVec2 zero;
	AEVec2Zero(&zero);

	AE_ASSERT_PARM(type < sGameObjNum);
	
	// loop through the object instance list to find a non-used object instance
	for (unsigned int i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjInst* pInst = sGameObjInstList + i;

		// check if current instance is not used
		if (pInst->flag == 0)
		{
			// it is not used => use it to create the new instance
			pInst->pObject			 = sGameObjList + type;
			pInst->flag				 = FLAG_ACTIVE | FLAG_VISIBLE;
			pInst->scale			 = scale;
			pInst->posCurr			 = pPos ? *pPos : zero;
			pInst->velCurr			 = pVel ? *pVel : zero;
			pInst->dirCurr			 = dir;
			pInst->pUserData		 = 0;
			pInst->gridCollisionFlag = 0;
			pInst->state			 = startState;
			pInst->innerState		 = INNER_STATE_ON_ENTER;
			pInst->counter			 = 0;
			
			// return the newly created instance
			return pInst;
		}
	}

	return 0;
}

/******************************************************************************/
/*!

*/
/******************************************************************************/
void gameObjInstDestroy(GameObjInst* pInst)
{
	// if instance is destroyed before, just return
	if (pInst->flag == 0)
		return;

	// zero out the flag
	pInst->flag = 0;
}

/******************************************************************************/
/*!

*/
/******************************************************************************/
int GetCellValue(int X, int Y)
{
	if (X < 0 || X >= BINARY_MAP_WIDTH || Y < 0 || Y >= BINARY_MAP_HEIGHT) {
		return 0;
	}
	return BinaryCollisionArray[X][Y];
}

/******************************************************************************/
/*!

*/
/******************************************************************************/
int CheckInstanceBinaryMapCollision(float PosX, float PosY, float scaleX, float scaleY)
{
	//At the end of this function, "Flag" will be used to determine which sides
	//of the object instance are colliding. 2 hot spots will be placed on each side.

	// up
	float ux1{ PosX + scaleX / 4.0f }, uy1{ PosY + scaleY / 2.0f },
		ux2{ PosX - scaleX / 4.0f }, uy2{ PosY + scaleY / 2.0f };
	// down
	float dx1{ PosX + scaleX / 4.0f }, dy1{ PosY - scaleY / 2.0f },
		dx2{ PosX - scaleX / 4.0f }, dy2{ PosY - scaleY / 2.0f };
	// left
	float lx1{ PosX - scaleX / 2.0f }, ly1{ PosY + scaleY / 4.0f },
		lx2{ PosX - scaleX / 2.0f }, ly2{ PosY - scaleY / 4.0f };
	// right
	float rx1{ PosX + scaleX / 2.0f }, ry1{ PosY + scaleY / 4.0f },
		rx2{ PosX + scaleX / 2.0f }, ry2{ PosY - scaleY / 4.0f };

	int flag = 0;
	// check if positions in occupied cell
	// up
	if (GetCellValue((int)ux1, (int)uy1) == TYPE_OBJECT_COLLISION || GetCellValue((int)ux2, (int)uy2) == TYPE_OBJECT_COLLISION) {
		flag |= COLLISION_TOP;
	}
	// down
	if (GetCellValue((int)dx1, (int)dy1) == TYPE_OBJECT_COLLISION || GetCellValue((int)dx2, (int)dy2) == TYPE_OBJECT_COLLISION) {
		flag |= COLLISION_BOTTOM;
	}
	// left
	if (GetCellValue((int)lx1, (int)ly1) == TYPE_OBJECT_COLLISION || GetCellValue((int)lx2, (int)ly2) == TYPE_OBJECT_COLLISION) {
		flag |= COLLISION_LEFT;
	}
	// right
	if (GetCellValue((int)rx1, (int)ry1) == TYPE_OBJECT_COLLISION || GetCellValue((int)rx2, (int)ry2) == TYPE_OBJECT_COLLISION) {
		flag |= COLLISION_RIGHT;
	}
	return flag;
}

/******************************************************************************/
/*!

*/
/******************************************************************************/
void SnapToCell(float *Coordinate)
{
	*Coordinate = (float)((int)(*Coordinate)) + 0.5f;
}

/******************************************************************************/
/*!

*/
/******************************************************************************/
int ImportMapDataFromFile(char *FileName)
{
	std::string line, s;
	int j;
	// open file 
	std::fstream file(FileName, std::ios::in);
	if (file) {
		file >> s >> BINARY_MAP_WIDTH >> s >> BINARY_MAP_HEIGHT;
		// allocate space
		MapData = new int* [BINARY_MAP_WIDTH];
		BinaryCollisionArray = new int* [BINARY_MAP_WIDTH];
		for (int i = 0; i < BINARY_MAP_WIDTH; ++i) {
			MapData[i] = new int[BINARY_MAP_HEIGHT];
			BinaryCollisionArray[i] = new int[BINARY_MAP_HEIGHT];
		}
		// add data in
		for (int y = 0; y < BINARY_MAP_HEIGHT; ++y) {
			for (int x = 0; x < BINARY_MAP_WIDTH; ++x) {
				file >> j;
				MapData[x][y] = j;
				BinaryCollisionArray[x][y] = j != TYPE_OBJECT_COLLISION ? 0 : 1;
			}
		}
		for (int y = 0; y < BINARY_MAP_HEIGHT; ++y) {
			for (int x = 0; x < BINARY_MAP_WIDTH; ++x) {
				int i = 0, n = y - 1, s = y + 1, w = x - 1, e = x + 1;
				if (n >= 0 && BinaryCollisionArray[x][n]) { i |= 1; }
				if (s < BINARY_MAP_HEIGHT && BinaryCollisionArray[x][s]) { i |= 8; }
				if (w >= 0 && BinaryCollisionArray[w][y]) { i |= 2; }
				if (e < BINARY_MAP_WIDTH && BinaryCollisionArray[e][y]) { i |= 4; }
				CellSpriteData[x][y] = i;
			}
		}
		return 1;
	}
	return 0;
}

/******************************************************************************/
/*!

*/
/******************************************************************************/
void FreeMapData(void)
{
	for (int i = 0; i < BINARY_MAP_WIDTH; ++i) {
		delete[] MapData[i];
		delete[] BinaryCollisionArray[i];
	}
	delete[] MapData;
	delete[] BinaryCollisionArray;
}

/******************************************************************************/
/*!

*/
/******************************************************************************/
void EnemyStateMachine(GameObjInst *pInst)
{
	/***********
	This state machine has 2 states: STATE_GOING_LEFT and STATE_GOING_RIGHT
	Each state has 3 inner states: INNER_STATE_ON_ENTER, INNER_STATE_ON_UPDATE, INNER_STATE_ON_EXIT
	Use "switch" statements to determine which state and inner state the enemy is currently in.


	STATE_GOING_LEFT
		INNER_STATE_ON_ENTER
			Set velocity X to -MOVE_VELOCITY_ENEMY
			Set inner state to "on update"

		INNER_STATE_ON_UPDATE
			If collision on left side OR bottom left cell is non collidable
				Initialize the counter to ENEMY_IDLE_TIME
				Set inner state to on exit
				Set velocity X to 0


		INNER_STATE_ON_EXIT
			Decrement counter by frame time
			if counter is less than 0 (sprite's idle time is over)
				Set state to "going right"
				Set inner state to "on enter"

	STATE_GOING_RIGHT is basically the same, with few modifications.

	***********/
	if (pInst) {
		bool check = false;
		switch (pInst->state) {
		case (STATE_GOING_LEFT):
			switch (pInst->innerState) {
			case (INNER_STATE_ON_ENTER):
				pInst->velCurr.x = -MOVE_VELOCITY_ENEMY;
				pInst->innerState = INNER_STATE_ON_UPDATE;
				break;
			case (INNER_STATE_ON_UPDATE):
				check = (pInst->posCurr.x - (int)pInst->posCurr.x <= 0.5f) ? !GetCellValue((int)pInst->posCurr.x - 1, (int)pInst->posCurr.y - 1) : false;
				if ((pInst->gridCollisionFlag & COLLISION_LEFT) == COLLISION_LEFT || check) {
					pInst->counter = ENEMY_IDLE_TIME;
					pInst->innerState = INNER_STATE_ON_EXIT;
					pInst->velCurr.x = 0;
				}
				break;
			case (INNER_STATE_ON_EXIT):
				pInst->counter -= g_dt;
				if (pInst->counter < 0.0) {
					pInst->state = STATE_GOING_RIGHT;
					pInst->innerState = INNER_STATE_ON_ENTER;
				}
				break;
			}
			break;
		case (STATE_GOING_RIGHT):
			switch (pInst->innerState) {
			case (INNER_STATE_ON_ENTER):
				pInst->velCurr.x = MOVE_VELOCITY_ENEMY;
				pInst->innerState = INNER_STATE_ON_UPDATE;
				break;
			case (INNER_STATE_ON_UPDATE):
				check = (pInst->posCurr.x - (int)pInst->posCurr.x >= 0.5f) ? !GetCellValue((int)pInst->posCurr.x + 1, (int)pInst->posCurr.y - 1) : false;
				if ((pInst->gridCollisionFlag & COLLISION_RIGHT) == COLLISION_RIGHT || check) {
					pInst->counter = ENEMY_IDLE_TIME;
					pInst->innerState = INNER_STATE_ON_EXIT;
					pInst->velCurr.x = 0;
				}
				break;
			case (INNER_STATE_ON_EXIT):
				pInst->counter -= g_dt;
				if (pInst->counter < 0.0) {
					pInst->state = STATE_GOING_LEFT;
					pInst->innerState = INNER_STATE_ON_ENTER;
				}
				break;
			}
			break;
		}
	}

	UNREFERENCED_PARAMETER(pInst);
}