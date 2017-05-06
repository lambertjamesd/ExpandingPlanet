#include "stdafx.h"
#include "PointList.h"
#include <algorithm>

void PointList::CreateResources()
{
	const SIZE_T dataSize = m_data.size() * sizeof(Particle);

	D3D12_HEAP_PROPERTIES defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	D3D12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	D3D12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(dataSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	D3D12_RESOURCE_DESC uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(dataSize);

	ThrowIfFailed(m_device.CreateCommittedResource(
		&defaultHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&m_particleBuffer)));

	ThrowIfFailed(m_device.CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&uploadBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_particleBufferUpload)));

	NAME_D3D12_OBJECT(m_particleBuffer);
}

PointList::PointList(const PointList& other) :
	m_device(other.m_device),
	m_commandList(other.m_commandList),
	m_data(other.m_data)
{
	CreateResources();
}

PointList::PointList(ID3D12Device& device, ID3D12GraphicsCommandList& commandList, UINT32 pointCount, float radius, float innerRadius) :
	m_device(device),
	m_commandList(commandList)
{
	// Initialize the data in the buffers.
	m_data.resize(pointCount);

	LoadParticles(&m_data[0], XMFLOAT3(0, 0, 0), radius, innerRadius, pointCount);

	CreateResources();
}


PointList::~PointList()
{
}


void PointList::PouplateBuffer()
{
	D3D12_SUBRESOURCE_DATA particleData = {};
	particleData.pData = reinterpret_cast<UINT8*>(&m_data[0]);
	particleData.RowPitch = m_data.size() * sizeof(Particle);
	particleData.SlicePitch = particleData.RowPitch;

	UpdateSubresources<1>(&m_commandList, m_particleBuffer.Get(), m_particleBufferUpload.Get(), 0, 0, 1, &particleData);
	m_commandList.ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_particleBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
}

// Random percent value, from -1 to 1.
float PointList::RandomPercent()
{
	float ret = static_cast<float>((rand() % 10000) - 5000);
	return ret / 5000.0f;
}

void PointList::LoadParticles(_Out_writes_(numParticles) Particle* pParticles, const XMFLOAT3& center, float spread, float innerRadius, UINT numParticles)
{
	srand(0);
	for (UINT i = 0; i < numParticles; i++)
	{
		XMFLOAT3 delta(spread, spread, spread);

		assert(spread > innerRadius);

		while (XMVectorGetX(XMVector3LengthSq(XMLoadFloat3(&delta))) > spread * spread || XMVectorGetX(XMVector3LengthSq(XMLoadFloat3(&delta))) < innerRadius * innerRadius)
		{
			delta.x = RandomPercent() * spread;
			delta.y = RandomPercent() * spread;
			delta.z = RandomPercent() * spread;
		}

		pParticles[i].position.x = center.x + delta.x;
		pParticles[i].position.y = center.y + delta.y;
		pParticles[i].position.z = center.z + delta.z;
		pParticles[i].position.w = 10000.0f * 10000.0f;

		pParticles[i].nextPoint = ~0;
	}

	/*std::sort(pParticles, pParticles + numParticles,
		[](const Particle & a, const Particle & b) -> bool
	{
		return a.position.x > b.position.x;
	});*/
}

ID3D12Resource* PointList::GetResource()
{
	return m_particleBuffer.Get();
}

UINT32 PointList::GetPointCount()
{
	return (UINT32)m_data.size();
}

XMFLOAT3 PointList::GetPoint(UINT32 index)
{
	if (index < m_data.size())
	{
		return XMFLOAT3(&m_data[index].position.x);
	}
	else
	{
		return XMFLOAT3(0.0f, 0.0f, 0.0f);
	}
}

void PointList::SetNextPoint(UINT32 from, UINT32 to)
{
	if (from < m_data.size() && (to < m_data.size() || to == ~0))
	{
		m_data[from].nextPoint = to;
	}
}

D3D12_SHADER_RESOURCE_VIEW_DESC PointList::SRVDesc()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = (UINT)m_data.size();
	srvDesc.Buffer.StructureByteStride = sizeof(Particle);
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	return srvDesc;
}

D3D12_UNORDERED_ACCESS_VIEW_DESC PointList::UAVDesc()
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = (UINT)m_data.size();
	uavDesc.Buffer.StructureByteStride = sizeof(Particle);
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
	return uavDesc;
}