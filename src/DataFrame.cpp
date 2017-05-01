#include "stdafx.h"
#include "DataFrame.h"

DataFrame::DataFrame()
{

}

void DataFrame::PopulateIndex(ID3D12GraphicsCommandList& commandList)
{
	m_spacialIndex->PopulateIndex(*m_pointList, commandList);
	m_pointList->PouplateBuffer();
}