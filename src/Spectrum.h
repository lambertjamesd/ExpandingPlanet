#pragma once

#include "DXSample.h"

using namespace DirectX;

using Microsoft::WRL::ComPtr;

class Spectrum
{
	CD3DX12_RESOURCE_DESC m_textureDesc;
	ID3D12Device& m_device;
	ID3D12GraphicsCommandList& m_commandList;

	ComPtr<ID3D12Resource> m_texture;
	ComPtr<ID3D12Resource> m_textureUpload;

	std::vector<UINT32> m_colorData;

	void UploadData();
	void WriteColor(UINT32 index, float r, float g, float b);
public:
	Spectrum(ID3D12Device& device, ID3D12GraphicsCommandList& commandList, UINT32 size);
	~Spectrum();

	void HueSpectrum(float maxHueDegress);

	ID3D12Resource* GetResource();

	void CreateView(ID3D12DescriptorHeap& heap, UINT32 index,UINT32 descriptorSize);
};

