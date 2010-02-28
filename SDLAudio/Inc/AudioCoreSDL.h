/*=============================================================================
	AudioCoreSDL.h: Core SDL audio function declarations.
	Copyright 1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Jeremy Muhlich
=============================================================================*/

/*------------------------------------------------------------------------------------
	Audio state control.
------------------------------------------------------------------------------------*/

INT OpenAudio( INT Rate, _WORD Format, INT PhysChannels );
void CloseAudio();
INT ReopenAudioDevice( INT Rate, _WORD Format, INT PhysChannels );

/*------------------------------------------------------------------------------------
	Audio flow control.
------------------------------------------------------------------------------------*/

void PlayAudio();

/*------------------------------------------------------------------------------------
	End.
------------------------------------------------------------------------------------*/
