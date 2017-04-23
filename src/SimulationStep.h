#pragma once

#include "DataFrame.h"
#include "SimulationParameters.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

class SimulationStep
{
	ID3D12Device& m_device;
	ID3D12GraphicsCommandList& m_commandList;

	UINT m_srvUavDescriptorSize;
	// Compute objects.
	ComPtr<ID3D12PipelineState> m_computeState;
	ComPtr<ID3D12CommandAllocator> m_computeAllocator;
	ComPtr<ID3D12CommandQueue> m_computeCommandQueue;
	ComPtr<ID3D12GraphicsCommandList> m_computeCommandList;
	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12DescriptorHeap> m_srvUavHeap;
	DataFrame* m_frames;
	SimulationParameters& m_parameters;

	void BuildRootSignature();
	void BuildComputeState(const std::wstring& shaderPath);

	enum RootParameterLocations {
		ConstantBufferView,
		SRVUAV,
		RootParameterCount
	};

	enum DataFrameStructure {
		ReadPointListIndex,
		ReadSpacialIndexIndex,
		WritePointListIndex,
		WriteSpacialIndexIndex,
		DataFrameSize
	};

	static const UINT32 FRAME_COUNT = 2;
public:
	SimulationStep(ID3D12Device& device, ID3D12GraphicsCommandList& commandList, DataFrame* frames, SimulationParameters& parameters, const std::wstring& shaderPath);
	~SimulationStep();

	void Step(UINT32 readFrame, ID3D12GraphicsCommandList& commandList);
};

