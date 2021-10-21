// Fill out your copyright notice in the Description page of Project Settings.

 
#pragma once

/**
 * 
 */
class CMarchingCubes
{
public:
	CMarchingCubes();
	~CMarchingCubes();

	static void  BuildTables();

	static char  m_CubeEdges[12][2];
	static char  m_CubeTriangles[256][16];
	static char  m_CubeNeighbors[256];
	static float m_CubeVertices[8][3];
};
