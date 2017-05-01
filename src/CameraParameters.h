#pragma once

using namespace DirectX;

using Microsoft::WRL::ComPtr;

class CameraParameters
{
	struct ConstantBuffer
	{
		XMFLOAT4X4 worldViewProjection;
		XMFLOAT4X4 inverseView;

		// Constant buffers are 256-byte aligned in GPU memory. Padding is added
		// for convenience when computing the struct's size.
		float padding[32];
	};

	ComPtr<ID3D12Resource> m_constantBuffer;
	ConstantBuffer* m_pConstantBufferData;
public:
	CameraParameters(ID3D12Device& device, UINT32 frameCount);
	~CameraParameters();
	D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress(UINT32 frameIndex);
	void Update(UINT32 frameIndex, const XMMATRIX& worldViewProjection, const XMMATRIX& inverseView);
};

