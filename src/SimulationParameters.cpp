#include "stdafx.h"
#include "SimulationParameters.h"
#include <math.h>

SimulationParameters::SimulationParameters(
	ID3D12Device& device,
	ID3D12GraphicsCommandList& commandList,
	float particleRadius,
	UINT32 particleCount
) : 
	m_device(device)
{
	const UINT constantBufferGSSize = sizeof(SimulationData);

	ThrowIfFailed(m_device.CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(constantBufferGSSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_constantBufferCS)
	));

	NAME_D3D12_OBJECT(m_constantBufferCS);

	CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
	ThrowIfFailed(m_constantBufferCS->Map(0, &readRange, reinterpret_cast<void**>(&m_data)));
	ZeroMemory(m_data, constantBufferGSSize);

	m_data->particleRadius = particleRadius;
	m_data->maxTime = 1.0f;
	m_data->coreRadius = 1.0f;
	m_data->currentTime = 0.0f;
	m_data->particleVelocity = 0.1f;
	m_data->solidPressure = 1.0f;
	m_data->cellWidth = std::fmax(particleRadius * 2.0f, 0.0f);
	m_data->indexSize = 10;
	m_data->particleCount = particleCount;
	m_data->currentBatch = 0;
}


SimulationParameters::~SimulationParameters()
{
}

void SimulationParameters::PostUpdate()
{
	++m_data->currentBatch;
}

ID3D12Resource* SimulationParameters::GetConstantBuffer()
{
	return m_constantBufferCS.Get();
}