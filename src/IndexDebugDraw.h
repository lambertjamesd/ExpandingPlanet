
#include "DataFrame.h"
#pragma once

#include "CameraParameters.h"
#include "DataFrame.h"
#include "SimulationParameters.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

class IndexDebugDraw
{
	enum RootSignature
	{
		GraphicsRootCBV,
		SRVTable,
		RootSignatureSize,
	};

	enum SRVValues
	{
		SRVPosVelo,
		SRVSpacialIndex,
		SRVValueCount,
	};

	ID3D12Device& m_device;
	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12DescriptorHeap> m_srvHeap;
	DataFrame* m_data;
	ComPtr<ID3D12PipelineState> m_pipelineState;
	CameraParameters& m_cameraParameters;
	DataFrame* m_dataFrames;
	SimulationParameters& m_simulationParameters;
	UINT32 m_srvUavDescriptorSize;

	static const UINT32 FRAME_COUNT = 2;

	void BuildRootSignature();
	void BuildSRV();
public:
	IndexDebugDraw(ID3D12Device& device, CameraParameters& cameraParameters, DataFrame* dataFrames, SimulationParameters& simulationParameters);
	~IndexDebugDraw();

	void BuildPipelineState(const std::wstring& vertexName, const std::wstring& geometryName, const std::wstring& pixelName);
	void Render(ID3D12GraphicsCommandList& commandList, UINT32 frameIndex, UINT32 sourceIndex);
};

