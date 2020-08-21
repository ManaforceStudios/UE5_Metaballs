
// FileName: MetaballsPlugin.h
// 
// Created by: Andrey Harchenko
// Project name: Metaballs FX Plugin
// Unreal Engine version: 4.10
// Created on: 2016/03/17
//
// -------------------------------------------------
// For parts referencing UE4 code, the following copyright applies:
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
//
// Feel free to use this software in any commercial/free game.
// Selling this as a plugin/item, in whole or part, is not allowed.
// See "License.md" for full licensing details.

#pragma once
 
#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"
 
class MetaballsPluginImpl : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	void StartupModule();
	void ShutdownModule();
};
