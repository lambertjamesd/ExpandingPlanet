#pragma once

#include "DXSample.h"

using namespace DirectX;

using Microsoft::WRL::ComPtr;

class PointList
{
public:
	struct Particle
	{
		XMFLOAT4 position;
		UINT32 nextPoint;
		XMUINT3 padding;
	};

private:
	ID3D12Device& m_device;
	ID3D12GraphicsCommandList& m_commandList;

	std::vector<Particle> m_data;
	ComPtr<ID3D12Resource> m_particleBuffer;
	ComPtr<ID3D12Resource> m_particleBufferUpload;

	float RandomPercent();
	void LoadParticles(_Out_writes_(numParticles) Particle* pParticles, const XMFLOAT3 &center, float spread, UINT numParticles);

public:
	PointList(ID3D12Device& device, ID3D12GraphicsCommandList& commandList, UINT32 pointCount, float radius);
	~PointList();

	void PouplateBuffer();

	ID3D12Resource* GetResource();

	UINT32 GetPointCount();
	XMFLOAT3 GetPoint(UINT32 index);
	void SetNextPoint(UINT32 from, UINT32 to);
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc();
	D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc();
};

