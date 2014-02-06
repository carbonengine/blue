#pragma once
#ifndef BlueNetTypes_H
#define BlueNetTypes_H
////////////////////////////////////////////////////////////
//
//	Creator:    Curt Hartung
//  Created:    Feb 2011
//  Copyright:  CCP 2011
//

//------------------------------------------------------------------------------
// this has been fixed for backward compatibility, in the future DO NOT
// USED THIS FILE TO DUMP MESSAGE TYPES, declare/use them local to your
// implementation

// todo- depricate this

#include "BlueNet.h"

const int BNT_ACTION_SEND_START_ACTION = BlueNet::BlueNetKeyFromName( "BlueNetTypes::BNT_ACTION_SEND_START_ACTION" );
const int BNT_ACTION_SEND_FORCE_ACTION = BlueNet::BlueNetKeyFromName( "BlueNetTypes::BNT_ACTION_SEND_FORCE_ACTION" );
const int BNT_ACTION_SEND_FORCE_ACTION_WITH_STEPS = BlueNet::BlueNetKeyFromName( "BlueNetTypes::BNT_ACTION_SEND_FORCE_ACTION_WITH_STEPS" );
const int BNT_ACTION_SEND_STEP_STATE = BlueNet::BlueNetKeyFromName( "BlueNetTypes::BNT_ACTION_SEND_STEP_STATE" );
const int BNT_ACTION_SEND_SERVER_INTERRUPT = BlueNet::BlueNetKeyFromName( "BlueNetTypes::BNT_ACTION_SEND_SERVER_INTERRUPT" );
const int BNT_ACTION_SEND_PROPERTY_UPDATE = BlueNet::BlueNetKeyFromName( "BlueNetTypes::BNT_ACTION_SEND_PROPERTY_UPDATE" );

const int BNT_MOVEMENT = BlueNet::BlueNetKeyFromName( "BlueNetTypes::BNT_MOVEMENT" );

const int BNT_BATMA_SEND_ATTRIBUTES = BlueNet::BlueNetKeyFromName( "BlueNetTypes::BNT_BATMA_SEND_ATTRIBUTES" );
const int BNT_BATMA_SEND_ADD_BUFF = BlueNet::BlueNetKeyFromName( "BlueNetTypes::BNT_BATMA_SEND_ADD_BUFF" );
const int BNT_BATMA_SEND_REMOVE_BUFF = BlueNet::BlueNetKeyFromName( "BlueNetTypes::BNT_BATMA_SEND_REMOVE_BUFF" );
	
const int BNT_PERCEPTION = BlueNet::BlueNetKeyFromName( "BlueNetTypes::BNT_PERCEPTION" );
const int BNT_AIMING = BlueNet::BlueNetKeyFromName( "BlueNetTypes::BNT_AIMING" );

#endif
