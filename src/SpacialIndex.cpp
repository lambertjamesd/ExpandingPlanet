#include "stdafx.h"
#include "SpacialIndex.h"
#include <math.h>

SpacialIndex::SpacialIndex(
	ID3D12Device& device, 
	ID3D12GraphicsCommandList& commandList,
	float cellSize, 
	UINT32 cellRowCount) : 
		m_device(device),
		m_commandList(commandList),
		m_cellSize(cellSize),
		m_cellRowCount(cellRowCount)
{
	m_cellCount = cellRowCount * cellRowCount * cellRowCount;
	UINT32 dataSize = sizeof(UINT32) * m_cellCount;
	std::vector<UINT32> data;
	data.resize(m_cellCount);
	memset(&data.front(), ~0, dataSize);

	D3D12_HEAP_PROPERTIES defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	D3D12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	D3D12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(dataSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	D3D12_RESOURCE_DESC uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(dataSize);

	ThrowIfFailed(device.CreateCommittedResource(
		&defaultHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&m_particleIndex)));

	ThrowIfFailed(device.CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&uploadBufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_particleUpload)));

	NAME_D3D12_OBJECT(m_particleIndex);
	NAME_D3D12_OBJECT(m_particleUpload);

	D3D12_SUBRESOURCE_DATA indexData = {};
	indexData.pData = reinterpret_cast<UINT8*>(&data[0]);
	indexData.RowPitch = dataSize;
	indexData.SlicePitch = indexData.RowPitch;

	UpdateSubresources<1>(&commandList, m_particleIndex.Get(), m_particleUpload.Get(), 0, 0, 1, &indexData);
	commandList.ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_particleIndex.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
}


SpacialIndex::~SpacialIndex()
{
}

ID3D12Resource* SpacialIndex::GetResource()
{
	return m_particleIndex.Get();
}

UINT32 SpacialIndex::SingleDimensionToIndex(float position)
{
	float origin = -m_cellSize * m_cellRowCount * 0.5f;
	UINT32 result = (UINT32)floorf((position - origin) / m_cellSize);
	return max(min(result, m_cellRowCount), 0);
}

UINT32 SpacialIndex::PositionToIndex(XMFLOAT3 position)
{
	return SingleDimensionToIndex(position.x) +
		(SingleDimensionToIndex(position.y) +
			SingleDimensionToIndex(position.z) * m_cellRowCount
			) * m_cellRowCount;
}

void SpacialIndex::PopulateIndex(PointList& pointList)
{
	UINT32 dataSize = sizeof(UINT32) * m_cellCount;
	std::vector<UINT32> data;
	data.resize(m_cellCount);
	memset(&data.front(), ~0, dataSize);

	for (UINT32 i = 0; i < pointList.GetPointCount(); ++i)
	{
		UINT32 cell = PositionToIndex(pointList.GetPoint(i));
		pointList.SetNextPoint(i, data[cell]);
		data[cell] = i;
	}
}

D3D12_SHADER_RESOURCE_VIEW_DESC SpacialIndex::SRVDesc()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = m_cellCount;
	srvDesc.Buffer.StructureByteStride = sizeof(UINT32);
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	return srvDesc;
}

D3D12_UNORDERED_ACCESS_VIEW_DESC SpacialIndex::UAVDesc()
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = m_cellCount;
	uavDesc.Buffer.StructureByteStride = sizeof(UINT32);
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
	return uavDesc;
}