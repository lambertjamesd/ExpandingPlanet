//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#pragma once

#include "DXSample.h"
#include "SimpleCamera.h"
#include "StepTimer.h"
#include "PointList.h"
#include "SpacialIndex.h"
#include "Spectrum.h"
#include "DataFrame.h"
#include "SimulationStep.h"
#include "CameraParameters.h"
#include "IndexDebugDraw.h"
#include <memory>

using namespace DirectX;

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().
using Microsoft::WRL::ComPtr;

class D3D12nBodyGravity : public DXSample
{
public:
	D3D12nBodyGravity(UINT width, UINT height, std::wstring name);

	virtual void OnInit();
	virtual void OnUpdate();
	virtual void OnRender();
	virtual void OnDestroy();
	virtual void OnKeyDown(UINT8 key);
	virtual void OnKeyUp(UINT8 key);

private:
	static const UINT FrameCount = 2;
	static const UINT ParticleCount = 1000000;		// The number of particles in the n-body simulation.

	// "Vertex" definition for particles. Triangle vertices are generated 
	// by the geometry shader. Color data will be assigned to those 
	// vertices via this struct.
	struct ParticleVertex
	{
		XMFLOAT4 color;
	};

	// Pipeline objects.
	CD3DX12_VIEWPORT m_viewport;
	CD3DX12_RECT m_scissorRect;
	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
	ComPtr<ID3D12Resource> m_depthStencil;
	UINT m_frameIndex;
	ComPtr<ID3D12CommandAllocator> m_commandAllocators[FrameCount];
	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
	ComPtr<ID3D12DescriptorHeap> m_srvUavHeap;
	ComPtr<ID3D12DescriptorHeap> m_srvHeap;
	UINT m_rtvDescriptorSize;
	UINT m_dsvDescriptorSize;
	UINT m_srvUavDescriptorSize;
	DXGI_FORMAT m_depthBufferFormat;

	std::unique_ptr<SimulationParameters> m_parameters;
	std::unique_ptr<SimulationStep> m_simulationState;
	std::unique_ptr<CameraParameters> m_cameraParameters;
	std::unique_ptr<IndexDebugDraw> m_indexDebugDraw;

	// Asset objects.
	ComPtr<ID3D12PipelineState> m_pipelineState;
	ComPtr<ID3D12GraphicsCommandList> m_commandList;
	ComPtr<ID3D12Resource> m_vertexBuffer;
	ComPtr<ID3D12Resource> m_vertexBufferUpload;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
	DataFrame m_dataFrames[FrameCount];
	std::unique_ptr<Spectrum> m_hueSpectrum;
	ComPtr<ID3D12Resource> m_particleNormal;
	ComPtr<ID3D12Resource> m_particleNormalUpload;

	UINT m_heightInstances;
	UINT m_widthInstances;
	SimpleCamera m_camera;
	StepTimer m_timer;

	// Compute objects.
	ComPtr<ID3D12CommandAllocator> m_computeAllocator;
	ComPtr<ID3D12CommandQueue> m_computeCommandQueue;
	ComPtr<ID3D12GraphicsCommandList> m_computeCommandList;

	// Synchronization objects.
	HANDLE m_swapChainEvent;
	ComPtr<ID3D12Fence> m_renderContextFence;
	UINT64 m_renderContextFenceValue;
	HANDLE m_renderContextFenceEvent;
	UINT64 m_frameFenceValues[FrameCount];

	ComPtr<ID3D12Fence> m_threadFences;
	volatile HANDLE m_threadFenceEvents;

	// Thread state.
	LONG volatile m_terminating;
	UINT64 volatile m_renderContextFenceValues;
	UINT64 volatile m_threadFenceValues;

	struct ThreadData
	{
		D3D12nBodyGravity* pContext;
		UINT threadIndex;
	};
	ThreadData m_threadData;
	HANDLE m_threadHandles;

	// Indices of the root signature parameters.
	enum GraphicsRootParameters : UINT32
	{
		GraphicsRootParameters = 0,
		GraphicsRootCBV,
		GraphicsRootSRVTable,
		GraphicsRootPixelTable,
		GraphicsRootParametersCount
	};

	enum ComputeRootParameters : UINT32
	{
		ComputeRootCBV = 0,
		ComputeRootSRVTable,
		ComputeRootUAVTable,
		ComputeRootParametersCount
	};

	// Indices of shader resources in the descriptor heap.
	enum DescriptorHeapIndex : UINT32
	{
		UavParticlePosVelo0 = 0,
		UavParticlePosVelo1,
		UavParticleIndex0,
		UavParticleIndex1,
		SrvParticlePosVelo0,
		SrvParticlePosVelo1,
		SrvParticleIndex0,
		SrvParticleIndex1,
		DescriptorCount
	};

	enum PixelDescriptorHeapIndex : UINT32
	{
		NormalMap = 0,
		SpectrumMap,
		PixelResourceCount
	};

	void LoadPipeline();
	void LoadAssets();
	void CreateAsyncContexts();
	void CreateVertexBuffer();
	void CreateParticleBuffers();
	void PopulateCommandList();

	static DWORD WINAPI ThreadProc(ThreadData* pData)
	{
		return pData->pContext->AsyncComputeThreadProc(pData->threadIndex);
	}
	DWORD AsyncComputeThreadProc(int threadIndex);
	void Simulate(UINT threadIndex);

	void WaitForRenderContext();
	void MoveToNextFrame();
};
