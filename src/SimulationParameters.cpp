#include "stdafx.h"
#include "SimulationParameters.h"


SimulationParameters::SimulationParameters()
{
}


SimulationParameters::~SimulationParameters()
{
}

ID3D12Resource* SimulationParameters::GetConstantBuffer()
{
	return m_constantBufferCS.Get();
}