#pragma once

#include "DXSample.h"
#include "PointList.h"

using namespace DirectX;

using Microsoft::WRL::ComPtr;

class SpacialIndex
{
	ID3D12Device& m_device;
	ID3D12GraphicsCommandList& m_commandList;

	float m_cellSize;
	UINT32 m_cellRowCount;
	UINT32 m_cellCount;

	ComPtr<ID3D12Resource> m_particleIndex;
	ComPtr<ID3D12Resource> m_particleUpload;

	UINT32 SingleDimensionToIndex(float position);
	UINT32 PositionToIndex(XMFLOAT3 position);
public:
	SpacialIndex(ID3D12Device& device, ID3D12GraphicsCommandList& commandList, float cellSize, UINT32 cellRowCount);
	~SpacialIndex();

	ID3D12Resource* GetResource();

	void PopulateIndex(PointList& pointList);
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc();
	D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc();
};

