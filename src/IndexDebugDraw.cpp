#include "stdafx.h"
#include "IndexDebugDraw.h"

void IndexDebugDraw::BuildRootSignature()
{
	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

	// This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

	if (FAILED(m_device.CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
	{
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	CD3DX12_DESCRIPTOR_RANGE1 ranges[SRVValueCount];
	ranges[SRVPosVelo].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
	ranges[SRVSpacialIndex].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

	CD3DX12_ROOT_PARAMETER1 rootParameters[RootSignatureSize];
	rootParameters[GraphicsRootCBV].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_ALL);
	rootParameters[SRVTable].InitAsDescriptorTable(SRVValueCount, ranges, D3D12_SHADER_VISIBILITY_ALL);

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC computeRootSignatureDesc;
	computeRootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr);

	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;
	auto result = D3DX12SerializeVersionedRootSignature(&computeRootSignatureDesc, featureData.HighestVersion, &signature, &error);
	ThrowIfFailed(result);
	ThrowIfFailed(m_device.CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
	NAME_D3D12_OBJECT(m_rootSignature);
}

void IndexDebugDraw::BuildSRV()
{
	D3D12_DESCRIPTOR_HEAP_DESC srvUavHeapDesc = {};
	srvUavHeapDesc.NumDescriptors = SRVValueCount * FRAME_COUNT;
	srvUavHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvUavHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(m_device.CreateDescriptorHeap(&srvUavHeapDesc, IID_PPV_ARGS(&m_srvHeap)));
	NAME_D3D12_OBJECT(m_srvHeap);

	auto descriptorHeap = m_srvHeap->GetCPUDescriptorHandleForHeapStart();

	for (UINT32 i = 0; i < FRAME_COUNT; ++i) {
		UINT32 offset = i * SRVValueCount;

		CD3DX12_CPU_DESCRIPTOR_HANDLE pointRead(descriptorHeap, SRVPosVelo + offset, m_srvUavDescriptorSize);
		CD3DX12_CPU_DESCRIPTOR_HANDLE indexRead(descriptorHeap, SRVSpacialIndex + offset, m_srvUavDescriptorSize);
		auto pointReadSRV = m_dataFrames[i].m_pointList->SRVDesc();
		auto indexReadSRV = m_dataFrames[i].m_spacialIndex->SRVDesc();
		m_device.CreateShaderResourceView(m_dataFrames[i].m_pointList->GetResource(), &pointReadSRV, pointRead);
		m_device.CreateShaderResourceView(m_dataFrames[i].m_spacialIndex->GetResource(), &indexReadSRV, indexRead);
	}
}

IndexDebugDraw::IndexDebugDraw(ID3D12Device& device, CameraParameters& cameraParameters, DataFrame* dataFrames, SimulationParameters& simulationParameters) :
	m_device(device),
	m_cameraParameters(cameraParameters),
	m_dataFrames(dataFrames),
	m_simulationParameters(simulationParameters)
{
	m_srvUavDescriptorSize = m_device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	BuildRootSignature();
	BuildSRV();
}

void IndexDebugDraw::BuildPipelineState(const std::wstring& vertexName, const std::wstring& geometryName, const std::wstring& pixelName)
{
	ComPtr<ID3DBlob> vertexShader;
	ComPtr<ID3DBlob> geometryShader;
	ComPtr<ID3DBlob> pixelShader;

	// Load and compile shaders.

	D3DReadFileToBlob(vertexName.c_str(), &vertexShader);
	D3DReadFileToBlob(geometryName.c_str(), &geometryShader);
	D3DReadFileToBlob(pixelName.c_str(), &pixelShader);

	// Describe and create the graphics pipeline state object (PSO).
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = { nullptr, 0 };
	psoDesc.pRootSignature = m_rootSignature.Get();
	psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
	psoDesc.GS = CD3DX12_SHADER_BYTECODE(geometryShader.Get());
	psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.SampleDesc.Count = 1;

	ThrowIfFailed(m_device.CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
	NAME_D3D12_OBJECT(m_pipelineState);
}

IndexDebugDraw::~IndexDebugDraw()
{
}

void IndexDebugDraw::Render(ID3D12GraphicsCommandList& commandList, UINT32 frameIndex, UINT32 sourceIndex)
{
	commandList.SetPipelineState(m_pipelineState.Get());
	commandList.SetGraphicsRootSignature(m_rootSignature.Get());

	commandList.SetGraphicsRootConstantBufferView(GraphicsRootCBV, m_cameraParameters.GetGPUVirtualAddress(frameIndex));

	ID3D12DescriptorHeap* pixelHeaps[] = { m_srvHeap.Get() };
	commandList.SetDescriptorHeaps(_countof(pixelHeaps), pixelHeaps);

	CD3DX12_GPU_DESCRIPTOR_HANDLE srvPixelHandle(m_srvHeap->GetGPUDescriptorHandleForHeapStart(), SRVValueCount * sourceIndex, m_srvUavDescriptorSize);
	commandList.SetGraphicsRootDescriptorTable(SRVTable, srvPixelHandle);

	commandList.IASetVertexBuffers(0, 0, nullptr);
	commandList.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);

	PIXBeginEvent(&commandList, 0, L"Draw spacial index debug");
	commandList.DrawInstanced(m_simulationParameters.IndexCellCount(), 1, 0, 0);
	PIXEndEvent(&commandList);
}