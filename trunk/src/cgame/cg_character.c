/*
**  Character loading
*/

#include "cg_local.h"

char bigTextBuffer[100000];

// TTimo - defined but not used
#if 0
/*
======================
CG_ParseGibModels

Read a configuration file containing gib models for use with this character
======================
*/
static qboolean	CG_ParseGibModels( bg_playerclass_t* classInfo ) {
	char		*text_p;
	int			len;
	int			i;
	char		*token;
	fileHandle_t	f;

	memset( classInfo->gibModels, 0, sizeof(classInfo->gibModels) );

	// load the file
	len = trap_FS_FOpenFile( va("models/players/%s/gibs.cfg", classInfo->modelPath), &f, FS_READ );
	if ( len <= 0 ) {
		return qfalse;
	}
	if ( len >= sizeof( bigTextBuffer ) - 1 ) {
		CG_Printf( "File %s too long\n", va("models/players/%s/gibs.cfg", classInfo->modelPath) );
		return qfalse;
	}
	trap_FS_Read( bigTextBuffer, len, f );
	bigTextBuffer[len] = 0;
	trap_FS_FCloseFile( f );

	// parse the text
	text_p = bigTextBuffer;

	for (i=0; i<MAX_GIB_MODELS; i++) {
		token = COM_Parse( &text_p );
		if ( !token ) {
			break;
		}
		// cache this model
		classInfo->gibModels[i] = trap_R_RegisterModel( token );
	}

	return qtrue;
}
#endif

/*
======================
CG_ParseHudHeadConfig
======================
*/
static qboolean	CG_ParseHudHeadConfig( const char *filename, animation_t* hha ) {
	char		*text_p;
	int			len;
	int			i;
	float		fps;
	char		*token;
	fileHandle_t	f;

	// load the file
	len = trap_FS_FOpenFile( filename, &f, FS_READ );
	if( len <= 0 ) {
		return qfalse;
	}

	if( len >= sizeof( bigTextBuffer ) - 1 ) {
		CG_Printf( "File %s too long\n", filename );
		return qfalse;
	}

	trap_FS_Read( bigTextBuffer, len, f );
	bigTextBuffer[len] = 0;
	trap_FS_FCloseFile( f );

	// parse the text
	text_p = bigTextBuffer;

	for ( i = 0 ; i < MAX_HD_ANIMATIONS ; i++ ) {
		token = COM_Parse( &text_p );	// first frame
		if( !token ) {
			break;
		}
		hha[i].firstFrame = atoi( token );

		token = COM_Parse( &text_p );	// length
		if( !token ) {
			break;
		}
		hha[i].numFrames = atoi( token );

		token = COM_Parse( &text_p );	// fps
		if( !token ) {
			break;
		}
		fps = atof( token );
		if ( fps == 0 ) {
			fps = 1;
		}

		hha[i].frameLerp = 1000 / fps;
		hha[i].initialLerp = 1000 / fps;

		token = COM_Parse( &text_p );	// looping frames
		if( !token ) {
			break;
		}
		hha[i].loopFrames = atoi( token );

		if( hha[i].loopFrames > hha[i].numFrames ) {
			hha[i].loopFrames = hha[i].numFrames;
		} else if( hha[i].loopFrames < 0 ) {
			hha[i].loopFrames = 0;
		}
	}

	if( i != MAX_HD_ANIMATIONS ) {
		CG_Printf( "Error parsing hud head animation file: %s", filename );
		return qfalse;
	}

	return qtrue;
}

/*
==================
CG_CalcMoveSpeeds
==================
*/
static void CG_CalcMoveSpeeds( bg_character_t *character )
{
	char			*tags[2] = {"tag_footleft", "tag_footright"};
	vec3_t			oldPos[2];
	refEntity_t		refent;
	animation_t		*anim;
	int				i, j, k;
	float			totalSpeed;
	int				numSpeed;
	int				lastLow, low;
	orientation_t	o[2];

	memset( &refent, 0, sizeof(refent) );

	refent.hModel = character->mesh;

	for( i = 0; i < character->animModelInfo->numAnimations; i++ ) {
		anim = character->animModelInfo->animations[i];

		if( anim->moveSpeed >= 0 ) {
			continue;
		}

		totalSpeed = 0;
		lastLow = -1;
		numSpeed = 0;

		// for each frame
		for( j = 0; j < anim->numFrames; j++ ) {

			refent.frame = anim->firstFrame + j;
			refent.oldframe = refent.frame;
			refent.torsoFrameModel = refent.oldTorsoFrameModel = refent.frameModel = refent.oldframeModel = anim->mdxFile;

			// for each foot
			for( k = 0; k < 2; k++ ) {
				if( trap_R_LerpTag( &o[k], &refent, tags[k], 0 ) < 0 ) {
					CG_Error( "CG_CalcMoveSpeeds: unable to find tag %s, cannot calculate movespeed", tags[k] );
				}
			}

			// find the contact foot
			if( anim->flags & ANIMFL_LADDERANIM ) {
				if( o[0].origin[0] > o[1].origin[0] )
					low = 0;
				else
					low = 1;
				totalSpeed += fabs( oldPos[low][2] - o[low].origin[2] );
			} else {
				if( o[0].origin[2] < o[1].origin[2] )
					low = 0;
				else
					low = 1;
				totalSpeed += fabs( oldPos[low][0] - o[low].origin[0] );
			}

			numSpeed++;

			// save the positions
			for( k = 0; k < 2; k++ ) {
				VectorCopy( o[k].origin, oldPos[k] );
			}
			lastLow = low;
		}

		// record the speed
		anim->moveSpeed = (int)((totalSpeed/numSpeed) * 1000.0 / anim->frameLerp);
	}
}

/*
======================
CG_ParseAnimationFiles

  Read in all the configuration and script files for this model.
======================
*/
static qboolean CG_ParseAnimationFiles( bg_character_t *character, const char *animationGroup, const char *animationScript )
{
	char			filename[MAX_QPATH];
	fileHandle_t	f;
	int				len;

	// set the name of the animationGroup and animationScript in the animModelInfo structure
	Q_strncpyz( character->animModelInfo->animationGroup, animationGroup, sizeof(character->animModelInfo->animationGroup) );
	Q_strncpyz( character->animModelInfo->animationScript, animationScript, sizeof(character->animModelInfo->animationScript) );

	BG_R_RegisterAnimationGroup( animationGroup, character->animModelInfo );

	// calc movespeed values if required
	CG_CalcMoveSpeeds( character );

	// load the script file
	len = trap_FS_FOpenFile( animationScript, &f, FS_READ );
	if( len <= 0 ) {
		return qfalse;
	}
	if ( len >= sizeof( bigTextBuffer ) - 1 ) {
		CG_Printf( "File %s is too long\n", filename );
		return qfalse;
	}
	trap_FS_Read( bigTextBuffer, len, f );
	bigTextBuffer[len] = 0;
	trap_FS_FCloseFile( f );

	// parse the text
	BG_AnimParseAnimScript( character->animModelInfo, &cgs.animScriptData, animationScript, bigTextBuffer );

	return qtrue;
}

/*
==================
CG_CheckForExistingAnimModelInfo

  If this player model has already been parsed, then use the existing information.
  Otherwise, set the modelInfo pointer to the first free slot.

  returns qtrue if existing model found, qfalse otherwise
==================
*/
static qboolean CG_CheckForExistingAnimModelInfo( const char *animationGroup, const char *animationScript, animModelInfo_t **animModelInfo )
{
	int i;
	animModelInfo_t *trav, *firstFree = NULL;

	for( i = 0, trav = cgs.animScriptData.modelInfo; i < MAX_ANIMSCRIPT_MODELS; i++, trav++ ) {
		if( *trav->animationGroup && *trav->animationScript ) {
			if( !Q_stricmp( trav->animationGroup, animationGroup ) && !Q_stricmp( trav->animationScript, animationScript ) ) {
				// found a match, use this animModelInfo
				*animModelInfo = trav;
				return qtrue;
			}
		} else if( !firstFree ) {
			firstFree = trav;
		}
	}

	if( !firstFree ) {
		CG_Error( "unable to find a free modelinfo slot, cannot continue\n" );
	} else {
		*animModelInfo = firstFree;
		// clear the structure out ready for use
		memset( *animModelInfo, 0, sizeof(*animModelInfo) );
	}

	// qfalse signifies that we need to parse the information from the script files
	return qfalse;
}

/*
==============
CG_RegisterAcc
==============
*/
static qboolean CG_RegisterAcc( const char *modelName, int *model, const char* skinname, qhandle_t* skin ) {
	char	filename[MAX_QPATH];

	*model = trap_R_RegisterModel( modelName );

	if( !*model )
		return qfalse;

	COM_StripExtensionSafe( modelName, filename, sizeof(filename) );
	Q_strcat( filename, sizeof(filename), va( "_%s.skin", skinname ) );

	*skin = trap_R_RegisterSkin( filename );

	return qtrue;
}

typedef struct {
	char		*type;
	accType_t	index;
} acc_t;

static acc_t cg_accessories[] = {
	{ "md3_beltr",		ACC_BELT_LEFT },
	{ "md3_beltl",		ACC_BELT_RIGHT },
	{ "md3_belt",		ACC_BELT },
	{ "md3_back",		ACC_BACK },
	{ "md3_weapon",		ACC_WEAPON },
	{ "md3_weapon2",	ACC_WEAPON2 },
};

static int cg_numAccessories = sizeof(cg_accessories) / sizeof(cg_accessories[0]);

static acc_t cg_headAccessories[] = {
	{ "md3_hat",		ACC_HAT },
	{ "md3_rank",		ACC_RANK },
	{ "md3_hat2",		ACC_MOUTH2 },
	{ "md3_hat3",		ACC_MOUTH3 },
};

static int cg_numHeadAccessories = sizeof(cg_headAccessories) / sizeof(cg_headAccessories[0]);

/*
====================
CG_RegisterCharacter
====================
*/
qboolean CG_RegisterCharacter( const char *characterFile, bg_character_t *character )
{
	bg_characterDef_t	characterDef;
	char *filename;
	char buf[MAX_QPATH];
	char accessoryname[MAX_QPATH];
	int					i;
	
	memset( &characterDef, 0, sizeof(characterDef) );

	if( !BG_ParseCharacterFile( characterFile, &characterDef ) ) {
		return qfalse;	// the parser will provide the error message
	}

	// Register Mesh
	if( !(character->mesh = trap_R_RegisterModel( characterDef.mesh )) )
		CG_Printf( S_COLOR_YELLOW "WARNING: failed to register mesh '%s' referenced from '%s'\n", characterDef.mesh, characterFile );

	// Register Skin
	COM_StripExtensionSafe( characterDef.mesh, buf, sizeof(buf) );
	filename = va( "%s_%s.skin", buf, characterDef.skin );

	if( !(character->skin = trap_R_RegisterSkin( filename )) ) {
		CG_Printf( S_COLOR_YELLOW "WARNING: failed to register skin '%s' referenced from '%s'\n", filename, characterFile );
	} else {
		for( i = 0; i < cg_numAccessories; i++ ) {
			if( trap_R_GetSkinModel( character->skin, cg_accessories[i].type, accessoryname ) ) {
				if( !CG_RegisterAcc( accessoryname, &character->accModels[cg_accessories[i].index], characterDef.skin, &character->accSkins[cg_accessories[i].index] ) ) {
					CG_Printf( S_COLOR_YELLOW "WARNING: failed to register accessory '%s' referenced from '%s'->'%s'\n", accessoryname, characterFile, filename );
				}
			}
		}

		for( i = 0; i < cg_numHeadAccessories; i++ ) {
			if( trap_R_GetSkinModel( character->skin, cg_headAccessories[i].type, accessoryname ) ) {
				if( !CG_RegisterAcc( accessoryname, &character->accModels[cg_headAccessories[i].index], characterDef.skin, &character->accSkins[cg_headAccessories[i].index] ) ) {
					CG_Printf( S_COLOR_YELLOW "WARNING: failed to register accessory '%s' referenced from '%s'->'%s'\n", accessoryname, characterFile, filename );
				}
			}
		}
	}

	// pheno: gib models
	character->gibModels[0] = trap_R_RegisterModel("models/gibs/foot.md3");
	character->gibModels[1] = trap_R_RegisterModel("models/gibs/foot.md3");
	character->gibModels[2] = trap_R_RegisterModel("models/gibs/leg.md3");
	character->gibModels[3] = trap_R_RegisterModel("models/gibs/leg.md3");
	character->gibModels[4] = trap_R_RegisterModel("models/gibs/abdomen.md3");
	character->gibModels[5] = trap_R_RegisterModel("models/gibs/intestine.md3");
	character->gibModels[6] = trap_R_RegisterModel("models/gibs/forearm.md3");
	character->gibModels[7] = trap_R_RegisterModel("models/gibs/forearm.md3");
	character->gibModels[8] = trap_R_RegisterModel("models/gibs/skull.md3");
	character->gibModels[9] = trap_R_RegisterModel("models/gibs/abdomen.md3");

	// Register Undressed Corpse Media
	if( *characterDef.undressedCorpseModel ) {
		// Register Undressed Corpse Model
		if( !(character->undressedCorpseModel = trap_R_RegisterModel( characterDef.undressedCorpseModel )) )
			CG_Printf( S_COLOR_YELLOW "WARNING: failed to register undressed corpse model '%s' referenced from '%s'\n", characterDef.undressedCorpseModel, characterFile );

		// Register Undressed Corpse Skin
		COM_StripExtensionSafe( characterDef.undressedCorpseModel,
			buf, sizeof(buf) );
		filename = va( "%s_%s.skin", buf, characterDef.undressedCorpseSkin );
		if( !(character->undressedCorpseSkin = trap_R_RegisterSkin( filename )) )
			CG_Printf( S_COLOR_YELLOW "WARNING: failed to register undressed corpse skin '%s' referenced from '%s'\n", filename, characterFile );
	}

	// Register the head for the hud
	if( *characterDef.hudhead ) {
		// Register Hud Head Model
		if( !(character->hudhead = trap_R_RegisterModel( characterDef.hudhead )) ) {
			CG_Printf( S_COLOR_YELLOW "WARNING: failed to register hud head model '%s' referenced from '%s'\n", characterDef.hudhead, characterFile );
		}

		if( *characterDef.hudheadskin && !(character->hudheadskin = trap_R_RegisterSkin( characterDef.hudheadskin )) ) {
			CG_Printf( S_COLOR_YELLOW "WARNING: failed to register hud head skin '%s' referenced from '%s'\n", characterDef.hudheadskin, characterFile );
		}

		if( *characterDef.hudheadanims ) {
			if( !CG_ParseHudHeadConfig( characterDef.hudheadanims, character->hudheadanimations ) ) {
				CG_Printf( S_COLOR_YELLOW "WARNING: failed to register hud head animations '%s' referenced from '%s'\n", characterDef.hudheadanims, characterFile );
			}
		} else {
			CG_Printf( S_COLOR_YELLOW "WARNING: no hud head animations supplied in '%s'\n", characterFile );
		}
	}

	// Parse Animation Files
	if( !CG_CheckForExistingAnimModelInfo( characterDef.animationGroup, characterDef.animationScript, &character->animModelInfo ) ) {
		if( !CG_ParseAnimationFiles( character, characterDef.animationGroup, characterDef.animationScript ) ) {
			CG_Printf( S_COLOR_YELLOW "WARNING: failed to load animation files referenced from '%s'\n", characterFile );
			return qfalse;
		}
	}

	// STILL MISSING: GIB MODELS (OPTIONAL?)

	return qtrue;
}

bg_character_t *CG_CharacterForClientinfo( clientInfo_t *ci, centity_t *cent )
{
	int		team, cls;

	if( cent && cent->currentState.eType == ET_CORPSE ) {
		if( cent->currentState.onFireStart >= 0 )
			return cgs.gameCharacters[ cent->currentState.onFireStart ];
		else {
			if( cent->currentState.modelindex < 4 )
				return BG_GetCharacter( cent->currentState.modelindex, cent->currentState.modelindex2, ci->characterType );
			else
				return BG_GetCharacter( cent->currentState.modelindex - 4, cent->currentState.modelindex2, ci->characterType );
		}
	}

	if( cent && cent->currentState.powerups & (1 << PW_OPS_DISGUISED) ) {
		team = ci->team == TEAM_AXIS ? TEAM_ALLIES : TEAM_AXIS;

		cls = ( cent->currentState.powerups >> PW_OPS_CLASS_1 ) & 7;

		return BG_GetCharacter( team, cls, ci->characterType );
	}

	if( ci->character ) {
		return ci->character;
	}

    return BG_GetCharacter( ci->team, ci->cls, ci->characterType );
}

bg_character_t *CG_CharacterForPlayerstate( playerState_t* ps ) {
	int	team, cls;

	if( ps->powerups[PW_OPS_DISGUISED] ) {
		team = cgs.clientinfo[ps->clientNum].team == TEAM_AXIS ? TEAM_ALLIES : TEAM_AXIS;

		cls = 0;
		if( ps->powerups[PW_OPS_CLASS_1] ) {
			cls |= 1;
		}
		if( ps->powerups[PW_OPS_CLASS_2] ) {
			cls |= 2;
		}
		if( ps->powerups[PW_OPS_CLASS_3] ) {
			cls |= 4;
		}

		return BG_GetCharacter( team, cls, cgs.clientinfo[ps->clientNum].characterType );
	}

	return BG_GetCharacter( cgs.clientinfo[ps->clientNum].team, cgs.clientinfo[ps->clientNum].cls, cgs.clientinfo[ps->clientNum].characterType );
}

/*
========================
CG_RegisterPlayerClasses
========================
*/
void CG_RegisterPlayerClasses( void )
{
	bg_playerclass_t	*classInfo;
	bg_character_t		*character;
	int					team, cls;

	for( team = TEAM_AXIS; team <= TEAM_ALLIES; team++ ) {
		for( cls = PC_SOLDIER; cls < NUM_PLAYER_CLASSES; cls++ ) {
			classInfo = BG_GetPlayerClassInfo( team, cls );
			character = BG_GetCharacter( team, cls, CHARTYPE_NORMAL );

			Q_strncpyz( character->characterFile, classInfo->characterFile, sizeof(character->characterFile) );

			if( !CG_RegisterCharacter( character->characterFile, character ) ) {
				CG_Error( "ERROR: CG_RegisterPlayerClasses: failed to load character file '%s' for the %s %s\n", character->characterFile, (team == TEAM_AXIS ? "Axis" : "Allied"), BG_ClassnameForNumber( classInfo->classNum ) );
			}

			if( !(classInfo->icon = trap_R_RegisterShaderNoMip( classInfo->iconName )) ) {
				CG_Printf( S_COLOR_YELLOW "WARNING: failed to load class icon '%s' for the %s %s\n", classInfo->iconName, (team == TEAM_AXIS ? "Axis" : "Allied"), BG_ClassnameForNumber( classInfo->classNum ) );
			}

			if( !(classInfo->arrow = trap_R_RegisterShaderNoMip( classInfo->iconArrow ))) {
				CG_Printf( S_COLOR_YELLOW "WARNING: failed to load icon arrow '%s' for the %s %s\n", classInfo->iconArrow, (team == TEAM_AXIS ? "Axis" : "Allied"), BG_ClassnameForNumber( classInfo->classNum ) );
			}
		}
	}
}

// this is to avoid memcpy on nested structures and pointers
void CG_CpyCharacter( bg_character_t *destchar, bg_character_t *srcchar )
{
	int i;

	memcpy( destchar->characterFile, srcchar->characterFile, sizeof( destchar->characterFile ) );

	destchar->mesh = srcchar->mesh;
	destchar->skin = srcchar->skin;

	destchar->headModel = srcchar->headModel;
	destchar->headSkin = srcchar->headSkin;

	for( i = 0; i < ACC_MAX; i++ ) {
		destchar->accModels[i] = srcchar->accModels[i];
	}

	for( i = 0; i < ACC_MAX; i++ ) {
		destchar->accSkins[i] = srcchar->accSkins[i];
	}

	for( i = 0; i < MAX_GIB_MODELS; i++ ) {
		destchar->gibModels[i] = srcchar->gibModels[i];
	}	

	destchar->undressedCorpseModel = srcchar->undressedCorpseModel;
	destchar->undressedCorpseSkin = srcchar->undressedCorpseSkin;

	destchar->hudhead = srcchar->hudhead;
	destchar->hudheadskin = srcchar->hudheadskin;

	for( i = 0; i < MAX_HD_ANIMATIONS; i++ ) {
#ifdef USE_MDXFILE
		destchar->hudheadanimations[i].mdxFile = srcchar->hudheadanimations[i].mdxFile;
#else
		memcpy( destchar->hudheadanimations[i].mdxFileName, srcchar->hudheadanimations[i].mdxFileName, sizeof( destchar->hudheadanimations[i].mdxFileName ) );
#endif
		memcpy( destchar->hudheadanimations[i].name, srcchar->hudheadanimations[i].name, sizeof( destchar->hudheadanimations[i].name ) );
		destchar->hudheadanimations[i].firstFrame = srcchar->hudheadanimations[i].firstFrame;
		destchar->hudheadanimations[i].numFrames = srcchar->hudheadanimations[i].numFrames;
		destchar->hudheadanimations[i].loopFrames = srcchar->hudheadanimations[i].loopFrames;
		destchar->hudheadanimations[i].frameLerp = srcchar->hudheadanimations[i].frameLerp;
		destchar->hudheadanimations[i].initialLerp = srcchar->hudheadanimations[i].initialLerp;
		destchar->hudheadanimations[i].moveSpeed = srcchar->hudheadanimations[i].moveSpeed;
		destchar->hudheadanimations[i].animBlend = srcchar->hudheadanimations[i].animBlend;
		destchar->hudheadanimations[i].duration = srcchar->hudheadanimations[i].duration;
		destchar->hudheadanimations[i].nameHash = srcchar->hudheadanimations[i].nameHash;
		destchar->hudheadanimations[i].flags = srcchar->hudheadanimations[i].flags;
		destchar->hudheadanimations[i].movetype = srcchar->hudheadanimations[i].movetype;
	}

	destchar->animModelInfo = srcchar->animModelInfo;
}

char *CG_GetSkinNameForClass( int cls )
{
	switch( cls ) {
	default:
	case PC_SOLDIER:
		return "soldier";
	case PC_MEDIC:
		return "medic";
	case PC_ENGINEER:
		return "engineer";
	case PC_FIELDOPS:
		return "fieldops";
	case PC_COVERTOPS:
		return "cvops";
	}
}

char *CG_GetSkinFileForCharType( int team, int cls, charType_t charType, skinFileType_t skinFileType )
{
	char *skinfile;
	char *teamdir;
	char *chartypedir;

	if( skinFileType == SKINFILETYPE_BODY ) {
		switch( cls ) {
		default:
		case PC_SOLDIER:
			skinfile = "body_soldier.skin";
			break;
		case PC_MEDIC:
			skinfile = "body_medic.skin";
			break;
		case PC_ENGINEER:
			skinfile = "body_engineer.skin";
			break;
		case PC_FIELDOPS:
			skinfile = "body_fieldops.skin";
			break;
		case PC_COVERTOPS:
			skinfile = "body_cvops.skin";
			break;
		}
	} else {
		switch( cls ) {
		default:
		case PC_SOLDIER:
			skinfile = "helmet_soldier.skin";
			break;
		case PC_MEDIC:
			skinfile = "helmet_medic.skin";
			break;
		case PC_ENGINEER:
			skinfile = "helmet_engineer.skin";
			break;
		case PC_FIELDOPS:
			skinfile = "helmet_fieldops.skin";
			break;
		case PC_COVERTOPS:
			skinfile = "cap_cvops.skin";
			break;
		}
	}

	switch( team ) {
	default:
	case TEAM_AXIS:
		teamdir = "ax";
		break;
	case TEAM_ALLIES:
		teamdir = "al";
		break;
	}

	switch( charType ) {
	default:
	case CHARTYPE_MEMBERS:
		chartypedir = "members";
		break;
	case CHARTYPE_ADMINS:
		chartypedir = "admins";
		break;
	}

	return va( "%s%c%s%c%s%c%s%c%s", "models", PATH_SEP, "players", PATH_SEP, chartypedir, PATH_SEP, teamdir, PATH_SEP, skinfile );
}

/*
====================
CG_LoadCharacterMods
====================
*/
// currently member and admin loops are seperated because we might do different things to them in future
// if it stays, we can make additional loop cycle and reduce lines of code a bit here
void CG_LoadCharacterMods( void )
{
	char skinfile[MAX_QPATH];
	int i, j, k;
	bg_character_t		*srcchar;
	bg_character_t		*destchar;
	char accessoryname[MAX_QPATH];

	if( !cgs.characterModsEnabled ) {
		return;
	}

	CG_LoadingString( "character mods" );

	// copy character values from standard character to admin and member characters
	for( i = TEAM_AXIS; i <= TEAM_ALLIES; i++ ) {
		for( j = PC_SOLDIER; j < NUM_PLAYER_CLASSES; j++ ) {
			srcchar = BG_GetCharacter( i, j, CHARTYPE_NORMAL );
			for( k = CHARTYPE_MEMBERS; k <= CHARTYPE_ADMINS; k++ ) {
				destchar = BG_GetCharacter( i, j, k );		
				CG_CpyCharacter( destchar, srcchar );
			}
		}
	}

	// load up member characters
	for( i = TEAM_AXIS; i <= TEAM_ALLIES; i++ ) {
		for( j = PC_SOLDIER; j < NUM_PLAYER_CLASSES; j++ ) {
			destchar = BG_GetCharacter( i, j, CHARTYPE_MEMBERS );

			Q_strncpyz( skinfile, CG_GetSkinFileForCharType( i, j, CHARTYPE_MEMBERS, SKINFILETYPE_BODY ), sizeof(skinfile) );

			if( !(destchar->skin = trap_R_RegisterSkin( skinfile )) ) {
				CG_Printf( S_COLOR_YELLOW "WARNING: failed to register skin '%s'\n", skinfile );
			} else {
				for( k = 0; k < cg_numAccessories; k++ ) {
					if( trap_R_GetSkinModel( destchar->skin, cg_accessories[k].type, accessoryname ) ) {						
						if( !CG_RegisterAcc( accessoryname, &destchar->accModels[cg_accessories[k].index], CG_GetSkinNameForClass(j), &destchar->accSkins[cg_accessories[k].index] ) ) {
							CG_Printf( S_COLOR_YELLOW "WARNING: failed to register accessory '%s' referenced from '%s'\n", accessoryname, skinfile );
						}
					}
				}

				for( k = 0; k < cg_numHeadAccessories; k++ ) {
					if( trap_R_GetSkinModel( destchar->skin, cg_headAccessories[k].type, accessoryname ) ) {						
						if( !CG_RegisterAcc( accessoryname, &destchar->accModels[cg_headAccessories[k].index], CG_GetSkinNameForClass(j), &destchar->accSkins[cg_headAccessories[k].index] ) ) {
							CG_Printf( S_COLOR_YELLOW "WARNING: failed to register accessory '%s' referenced from '%s'\n", accessoryname, skinfile );
						}
					}
				}
			}			
		}
	}

	// load up admin characters
	for( i = TEAM_AXIS; i <= TEAM_ALLIES; i++ ) {
		for( j = PC_SOLDIER; j < NUM_PLAYER_CLASSES; j++ ) {
			destchar = BG_GetCharacter( i, j, CHARTYPE_ADMINS );

			Q_strncpyz( skinfile, CG_GetSkinFileForCharType( i, j, CHARTYPE_ADMINS, SKINFILETYPE_BODY ), sizeof(skinfile) );

			if( !(destchar->skin = trap_R_RegisterSkin( skinfile )) ) {
				CG_Printf( S_COLOR_YELLOW "WARNING: failed to register skin '%s'\n", skinfile );
			} else {
				for( k = 0; k < cg_numAccessories; k++ ) {
					if( trap_R_GetSkinModel( destchar->skin, cg_accessories[k].type, accessoryname ) ) {						
						if( !CG_RegisterAcc( accessoryname, &destchar->accModels[cg_accessories[k].index], CG_GetSkinNameForClass(j), &destchar->accSkins[cg_accessories[k].index] ) ) {
							CG_Printf( S_COLOR_YELLOW "WARNING: failed to register accessory '%s' referenced from '%s'\n", accessoryname, skinfile );
						}
					}
				}

				for( k = 0; k < cg_numHeadAccessories; k++ ) {
					if( trap_R_GetSkinModel( destchar->skin, cg_headAccessories[k].type, accessoryname ) ) {						
						if( !CG_RegisterAcc( accessoryname, &destchar->accModels[cg_headAccessories[k].index], CG_GetSkinNameForClass(j), &destchar->accSkins[cg_headAccessories[k].index] ) ) {
							CG_Printf( S_COLOR_YELLOW "WARNING: failed to register accessory '%s' referenced from '%s'\n", accessoryname, skinfile );
						}
					}
				}
			}
		}
	}
}
