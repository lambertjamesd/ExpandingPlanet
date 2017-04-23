#include "stdafx.h"
#include "DataFrame.h"

DataFrame::DataFrame()
{

}

void DataFrame::PopulateIndex()
{
	m_spacialIndex->PopulateIndex(*m_pointList);
	m_pointList->PouplateBuffer();
}