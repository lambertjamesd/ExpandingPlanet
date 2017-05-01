#include "stdafx.h"
#include "CameraParameters.h"

#include "DXSample.h"

CameraParameters::CameraParameters(ID3D12Device& device, UINT32 frameCount)
{
	const UINT constantBufferGSSize = sizeof(ConstantBuffer) * frameCount;

	ThrowIfFailed(device.CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(constantBufferGSSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_constantBuffer)
	));

	NAME_D3D12_OBJECT(m_constantBuffer);

	CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
	ThrowIfFailed(m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_pConstantBufferData)));
	ZeroMemory(m_pConstantBufferData, constantBufferGSSize);
}


CameraParameters::~CameraParameters()
{
}

D3D12_GPU_VIRTUAL_ADDRESS CameraParameters::GetGPUVirtualAddress(UINT32 frameIndex)
{
	return m_constantBuffer->GetGPUVirtualAddress() + frameIndex * sizeof(ConstantBuffer);
}

void CameraParameters::Update(UINT32 frameIndex, const XMMATRIX& worldViewProjection, const XMMATRIX& inverseView)
{
	XMStoreFloat4x4(&m_pConstantBufferData[frameIndex].worldViewProjection, worldViewProjection);
	XMStoreFloat4x4(&m_pConstantBufferData[frameIndex].inverseView, inverseView);
}