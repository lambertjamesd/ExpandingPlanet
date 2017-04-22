#include "stdafx.h"
#include "Spectrum.h"


Spectrum::Spectrum(ID3D12Device& device, ID3D12GraphicsCommandList& commandList, UINT32 size) :
	m_device(device),
	m_commandList(commandList)
{
	m_colorData.resize(size);

	m_textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, size, 1, 1, 1);

	ThrowIfFailed(device.CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&m_textureDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&m_texture)
	));

	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_texture.Get(), 0, 1) + D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;

	ThrowIfFailed(device.CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_textureUpload)));
}


Spectrum::~Spectrum()
{
}


void Spectrum::UploadData()
{
	D3D12_SUBRESOURCE_DATA textureData = {};
	textureData.pData = reinterpret_cast<UINT8*>(m_colorData.data());
	textureData.RowPitch = m_colorData.size() * sizeof(UINT32);
	textureData.SlicePitch = textureData.RowPitch;

	UpdateSubresources<1>(&m_commandList, m_texture.Get(), m_textureUpload.Get(), 0, 0, 1, &textureData);
	NAME_D3D12_OBJECT(m_texture);
}

void Spectrum::WriteColor(UINT32 index, float r, float g, float b)
{
	UINT32 rUInt = (UINT32)(max(min(r, 1.0f), 0.0f) * 255.0f);
	UINT32 gUInt = (UINT32)(max(min(g, 1.0f), 0.0f) * 255.0f);
	UINT32 bUInt = (UINT32)(max(min(b, 1.0f), 0.0f) * 255.0f);

	m_colorData[index] = rUInt | (gUInt << 8) | (bUInt << 16) | 0xFF000000;
}

void Spectrum::HueSpectrum(float maxHueDegress)
{
	for (UINT32 i = 0; i < m_colorData.size(); ++i)
	{
		float hue = maxHueDegress * i / m_colorData.size();

		float X = (1.0f - std::fabs(fmodf(hue / 60.0f, 2.0f) - 1.0f));

		if (hue < 60.0f)
		{
			WriteColor(i, 1.0f, X, 0.0f);
		}
		else if (hue < 120.0f)
		{
			WriteColor(i, X, 1.0f, 0.0f);
		}
		else if (hue < 180.0f)
		{
			WriteColor(i, 0.0f, 1.0f, X);
		}
		else if (hue < 240.0f)
		{
			WriteColor(i, 0.0f, X, 1.0f);
		}
		else if (hue < 300.0f)
		{
			WriteColor(i, X, 0.0f, 1.0f);
		}
		else
		{
			WriteColor(i, 1.0f, 0.0f, X);
		}
	}

	UploadData();
}

ID3D12Resource* Spectrum::GetResource()
{
	return m_texture.Get();
}

void Spectrum::CreateView(ID3D12DescriptorHeap& heap, UINT32 index, UINT32 descriptorSize)
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE spectrumHandle(heap.GetCPUDescriptorHandleForHeapStart(), index, descriptorSize);
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = m_textureDesc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = m_textureDesc.MipLevels;
	m_device.CreateShaderResourceView(m_texture.Get(), &srvDesc, spectrumHandle);
}