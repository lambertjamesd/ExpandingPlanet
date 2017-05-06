#include "stdafx.h"
#include "SimulationParameters.h"

#define _USE_MATH_DEFINES
#include <math.h>

SimulationParameters::SimulationParameters(
	ID3D12Device& device,
	ID3D12GraphicsCommandList& commandList,
	float particleRadius,
	UINT32 particleCount,
	float percentHollow,
	float expandRatio,
	float maxTime,
	float timestep
) : 
	m_device(device),
	m_timeStep(timestep)
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

	float allParticleRadius = ParticleToSimRadius(particleRadius, particleCount);
	float particleVolume = RadiusToVolume(allParticleRadius);
	float emptyVolume = particleVolume * percentHollow;

	m_startOuterRadius = VolumeToRadius(particleVolume + emptyVolume);

	float endRadius = expandRatio * m_startOuterRadius;
	m_maxInnerRadius = VolumeToRadius(RadiusToVolume(endRadius) - allParticleRadius);
	m_minInnerRadius = VolumeToRadius(emptyVolume);

	m_data->particleRadius = particleRadius;
	m_data->maxTime = maxTime;
	m_data->coreRadius = m_minInnerRadius;
	m_data->currentTime = 0.0f;
	m_data->particleVelocity = 0.1f;
	m_data->solidPressure = 1.0f;
	m_data->cellWidth = particleRadius * 2.0f;
	m_data->indexSize = (UINT32)std::ceil(endRadius / particleRadius);
	m_data->particleCount = particleCount;
	m_data->currentBatch = 0;
}


SimulationParameters::~SimulationParameters()
{
}

float SimulationParameters::ParticleToSimRadius(float particleRadius, UINT32 particleCount)
{
	float ratio = 1.1053389142800113968124644266762f;
	return std::pow((float)particleCount, 1.0f / 3.0f) * ratio * particleRadius;
}

float SimulationParameters::RadiusToVolume(float radius)
{
	return (4.0f / 3.0f) * XM_PI * radius * radius * radius;
}

float SimulationParameters::VolumeToRadius(float volume)
{
	return std::pow(volume * 3.0f / (4.0f * XM_PI), 1.0f / 3.0f);
}

void SimulationParameters::PostUpdate()
{
	++m_data->currentBatch;

	if (m_data->currentTime < m_data->maxTime) {
		m_data->currentTime = min(m_data->maxTime, m_data->currentTime += m_timeStep);

		float percentTime = (m_data->currentTime / m_data->maxTime);

		percentTime = (std::exp(percentTime) - 1.0f) / ((float)M_E - 1.0f);

		m_data->coreRadius = (m_maxInnerRadius - m_minInnerRadius) * percentTime + m_minInnerRadius;
	}
}

ID3D12Resource* SimulationParameters::GetConstantBuffer()
{
	return m_constantBufferCS.Get();
}

SimulationParameters::SimulationData& SimulationParameters::GetData()
{
	return *m_data;
}

UINT32 SimulationParameters::IndexCellCount()
{
	return m_data->indexSize * m_data->indexSize * m_data->indexSize;
}

float SimulationParameters::GetStartRadius()
{
	return m_startOuterRadius;
}