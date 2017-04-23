#include "stdafx.h"
#include "SimulationStep.h"
#include "DXSample.h"

void SimulationStep::BuildRootSignature()
{
	m_srvUavDescriptorSize = m_device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

	// This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

	if (FAILED(m_device.CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
	{
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	CD3DX12_DESCRIPTOR_RANGE1 ranges[DataFrameSize];

	ranges[ReadPointListIndex].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
	ranges[ReadSpacialIndexIndex].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
	ranges[WritePointListIndex].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
	ranges[WriteSpacialIndexIndex].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);

	CD3DX12_ROOT_PARAMETER1 rootParameters[RootParameterCount];
	rootParameters[ConstantBufferView].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_ALL);
	rootParameters[SRVUAV].InitAsDescriptorTable(DataFrameSize, ranges, D3D12_SHADER_VISIBILITY_ALL);

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC computeRootSignatureDesc;
	computeRootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr);

	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;
	ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&computeRootSignatureDesc, featureData.HighestVersion, &signature, &error));
	ThrowIfFailed(m_device.CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
	NAME_D3D12_OBJECT(m_rootSignature);

	D3D12_DESCRIPTOR_HEAP_DESC srvUavHeapDesc = {};
	srvUavHeapDesc.NumDescriptors = DataFrameSize * FRAME_COUNT;
	srvUavHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvUavHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(m_device.CreateDescriptorHeap(&srvUavHeapDesc, IID_PPV_ARGS(&m_srvUavHeap)));
	NAME_D3D12_OBJECT(m_srvUavHeap);

	auto descriptorHeap = m_srvUavHeap->GetCPUDescriptorHandleForHeapStart();

	for (UINT32 i = 0; i < FRAME_COUNT; ++i) {
		UINT32 offset = i * DataFrameSize;
		CD3DX12_CPU_DESCRIPTOR_HANDLE pointRead(descriptorHeap, ReadPointListIndex + offset, m_srvUavDescriptorSize);
		CD3DX12_CPU_DESCRIPTOR_HANDLE indexRead(descriptorHeap, ReadSpacialIndexIndex + offset, m_srvUavDescriptorSize);
		CD3DX12_CPU_DESCRIPTOR_HANDLE pointWrite(descriptorHeap, WritePointListIndex + offset, m_srvUavDescriptorSize);
		CD3DX12_CPU_DESCRIPTOR_HANDLE indexWrite(descriptorHeap, WriteSpacialIndexIndex + offset, m_srvUavDescriptorSize);
		UINT32 writeIndex = (i + 1) % FRAME_COUNT; 
		auto pointReadSRV = m_frames[i].m_pointList->SRVDesc();
		auto indexReadSRV = m_frames[i].m_spacialIndex->SRVDesc();
		auto pointWriteUAV = m_frames[writeIndex].m_pointList->UAVDesc();
		auto indexWriteUAV = m_frames[writeIndex].m_spacialIndex->UAVDesc();
		m_device.CreateShaderResourceView(m_frames[i].m_pointList->GetResource(), &pointReadSRV, pointRead);
		m_device.CreateShaderResourceView(m_frames[i].m_spacialIndex->GetResource(), &indexReadSRV, indexRead);
		m_device.CreateUnorderedAccessView(m_frames[writeIndex].m_pointList->GetResource(), nullptr, &pointWriteUAV, pointWrite);
		m_device.CreateUnorderedAccessView(m_frames[writeIndex].m_spacialIndex->GetResource(), nullptr, &indexWriteUAV, indexWrite);
	}
}

void SimulationStep::BuildComputeState(const std::wstring& shaderPath)
{
	ComPtr<ID3DBlob> computeShader;
	
#if defined(_DEBUG)
	// Enable better shader debugging with the graphics debugging tools.
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif

	D3DReadFileToBlob(shaderPath.c_str(), &computeShader);

	// Describe and create the compute pipeline state object (PSO).
	D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
	computePsoDesc.pRootSignature = m_rootSignature.Get();
	computePsoDesc.CS = CD3DX12_SHADER_BYTECODE(computeShader.Get());

	ThrowIfFailed(m_device.CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&m_computeState)));
	NAME_D3D12_OBJECT(m_computeState);
}



SimulationStep::SimulationStep(ID3D12Device& device, ID3D12GraphicsCommandList& commandList, DataFrame* frames, SimulationParameters& parameters, const std::wstring& shaderPath) :
	m_device(device),
	m_commandList(commandList),
	m_frames(frames),
	m_parameters(parameters)
{
	BuildRootSignature();
	BuildComputeState(shaderPath);
}


SimulationStep::~SimulationStep()
{
}

void SimulationStep::Step(UINT32 readFrame, ID3D12GraphicsCommandList& commandList)
{
	UINT32 writeFrame = (readFrame + 1) % FRAME_COUNT;

	commandList.ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_frames[writeFrame].m_pointList->GetResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
	commandList.ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_frames[writeFrame].m_spacialIndex->GetResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

	commandList.SetPipelineState(m_computeState.Get());
	commandList.SetComputeRootSignature(m_rootSignature.Get());

	ID3D12DescriptorHeap* ppHeaps[] = { m_srvUavHeap.Get() };
	commandList.SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle(m_srvUavHeap->GetGPUDescriptorHandleForHeapStart(), readFrame * DataFrameSize, m_srvUavDescriptorSize);

	commandList.SetComputeRootConstantBufferView(ConstantBufferView, m_parameters.GetConstantBuffer()->GetGPUVirtualAddress());
	commandList.SetComputeRootDescriptorTable(SRVUAV, srvHandle);

	commandList.Dispatch(static_cast<int>(ceil(m_parameters.GetData().particleCount / 128.0f)), 1, 1);

	commandList.ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_frames[writeFrame].m_pointList->GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
	commandList.ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_frames[writeFrame].m_spacialIndex->GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

}

void SimulationStep::Reset(ID3D12GraphicsCommandList& commandList, ID3D12CommandAllocator& commandAllocator)
{
	ThrowIfFailed(commandList.Reset(&commandAllocator, m_computeState.Get()));
}