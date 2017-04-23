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

#include "stdafx.h"
#include "D3D12nBodyGravity.h"
#include <math.h>

// InterlockedCompareExchange returns the object's value if the 
// comparison fails.  If it is already 0, then its value won't 
// change and 0 will be returned.
#define InterlockedGetValue(object) InterlockedCompareExchange(object, 0, 0)

const float D3D12nBodyGravity::ParticleSpread = 400.0f;

D3D12nBodyGravity::D3D12nBodyGravity(UINT width, UINT height, std::wstring name) :
	DXSample(width, height, name),
	m_frameIndex(0),
	m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
	m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
	m_rtvDescriptorSize(0),
	m_dsvDescriptorSize(0),
	m_srvUavDescriptorSize(0),
	m_depthBufferFormat(DXGI_FORMAT_D16_UNORM),
	m_pConstantBufferGSData(nullptr),
	m_renderContextFenceValue(0),
	m_terminating(0),
	m_frameFenceValues{}
{
	m_renderContextFenceValues = 0;
	m_threadFenceValues = 0;

	m_heightInstances = 1;
	m_widthInstances = 1;
}

void D3D12nBodyGravity::OnInit()
{
	m_camera.Init({ 0.0f, 0.0f, 1500.0f });
	m_camera.SetMoveSpeed(250.0f);

	LoadPipeline();
	LoadAssets();
	CreateAsyncContexts();
}

// Load the rendering pipeline dependencies.
void D3D12nBodyGravity::LoadPipeline()
{
	UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
	// Enable the debug layer (requires the Graphics Tools "optional feature").
	// NOTE: Enabling the debug layer after device creation will invalidate the active device.
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();

			// Enable additional debug layers.
			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
	}
#endif

	ComPtr<IDXGIFactory4> factory;
	ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

	if (m_useWarpDevice)
	{
		ComPtr<IDXGIAdapter> warpAdapter;
		ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

		ThrowIfFailed(D3D12CreateDevice(
			warpAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&m_device)
			));
	}
	else
	{
		ComPtr<IDXGIAdapter1> hardwareAdapter;
		GetHardwareAdapter(factory.Get(), &hardwareAdapter);

		ThrowIfFailed(D3D12CreateDevice(
			hardwareAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&m_device)
			));
	}

	// Describe and create the command queue.
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));
	NAME_D3D12_OBJECT(m_commandQueue);

	// Describe and create the swap chain.
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = FrameCount;
	swapChainDesc.Width = m_width;
	swapChainDesc.Height = m_height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

	ComPtr<IDXGISwapChain1> swapChain;
	ThrowIfFailed(factory->CreateSwapChainForHwnd(
		m_commandQueue.Get(),		// Swap chain needs the queue so that it can force a flush on it.
		Win32Application::GetHwnd(),
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain
		));

	// This sample does not support fullscreen transitions.
	ThrowIfFailed(factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));

	ThrowIfFailed(swapChain.As(&m_swapChain));
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
	m_swapChainEvent = m_swapChain->GetFrameLatencyWaitableObject();

	// Create descriptor heaps.
	{
		// Describe and create a render target view (RTV) descriptor heap.
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = FrameCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

		// Describe and create a depth view (DSV) descriptor heap
		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
		dsvHeapDesc.NumDescriptors = 1;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));
		NAME_D3D12_OBJECT(m_dsvHeap);

		// Describe and create a shader resource view (SRV) and unordered
		// access view (UAV) descriptor heap.
		D3D12_DESCRIPTOR_HEAP_DESC srvUavHeapDesc = {};
		srvUavHeapDesc.NumDescriptors = DescriptorCount;
		srvUavHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvUavHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&srvUavHeapDesc, IID_PPV_ARGS(&m_srvUavHeap)));
		NAME_D3D12_OBJECT(m_srvUavHeap);

		D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
		srvHeapDesc.NumDescriptors = PixelResourceCount;
		srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap)));
		NAME_D3D12_OBJECT(m_srvHeap);

		m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		m_dsvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
		m_srvUavDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	// Create frame resources.
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

		// Create a RTV and a command allocator for each frame.
		for (UINT n = 0; n < FrameCount; n++)
		{
			ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
			m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
			rtvHandle.Offset(1, m_rtvDescriptorSize);

			NAME_D3D12_OBJECT_INDEXED(m_renderTargets, n);

			ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[n])));
		}
	}

	// Create a depth stencil and view.
	{
		D3D12_HEAP_PROPERTIES depthHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

		D3D12_RESOURCE_DESC depthResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(m_depthBufferFormat, m_width, m_height, 1, 1);
		depthResourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		CD3DX12_CLEAR_VALUE depthOptimizedClearValue(m_depthBufferFormat, 1.0f, 0);

		ThrowIfFailed(m_device->CreateCommittedResource(
			&depthHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&depthResourceDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&depthOptimizedClearValue,
			IID_PPV_ARGS(&m_depthStencil)
		));

		NAME_D3D12_OBJECT(m_depthStencil);

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = m_depthBufferFormat;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

		m_device->CreateDepthStencilView(m_depthStencil.Get(), &dsvDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
	}
}

// Load the sample assets.
void D3D12nBodyGravity::LoadAssets()
{
	// Create the root signatures.
	{
		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

		// This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

		if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		{
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

		// Graphics root signature.
		{
			CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
			ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

			CD3DX12_DESCRIPTOR_RANGE1 pixelRanges[2];
			pixelRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
			pixelRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
			
			CD3DX12_ROOT_PARAMETER1 rootParameters[GraphicsRootParametersCount];
			rootParameters[GraphicsRootCBV].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_ALL);
			rootParameters[GraphicsRootSRVTable].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);
			rootParameters[GraphicsRootPixelTable].InitAsDescriptorTable(2, &pixelRanges[0], D3D12_SHADER_VISIBILITY_ALL);

			CD3DX12_STATIC_SAMPLER_DESC samplerDesc(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);

			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
			rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &samplerDesc, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

			ComPtr<ID3DBlob> signature;
			ComPtr<ID3DBlob> error;
			ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
			ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
			NAME_D3D12_OBJECT(m_rootSignature);
		}
	}

	// Create the pipeline states, which includes compiling and loading shaders.
	{
		ComPtr<ID3DBlob> vertexShader;
		ComPtr<ID3DBlob> geometryShader;
		ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif

		// Load and compile shaders.
		ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"ParticleDraw.hlsl").c_str(), nullptr, nullptr, "VSParticleDraw", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
		ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"ParticleDraw.hlsl").c_str(), nullptr, nullptr, "GSParticleDraw", "gs_5_0", compileFlags, 0, &geometryShader, nullptr));
		ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"ParticleDraw.hlsl").c_str(), nullptr, nullptr, "PSParticleDraw", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));

		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		// Describe and create the graphics pipeline state object (PSO).
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
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

		ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
		NAME_D3D12_OBJECT(m_pipelineState);
	}

	// Create the command list.
	ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[m_frameIndex].Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));
	NAME_D3D12_OBJECT(m_commandList);

	CreateVertexBuffer();
	CreateParticleBuffers();

	// Create the geometry shader's constant buffer.
	{
		const UINT constantBufferGSSize = sizeof(ConstantBufferGS) * FrameCount;

		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(constantBufferGSSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_constantBufferGS)
			));

		NAME_D3D12_OBJECT(m_constantBufferGS);

		CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
		ThrowIfFailed(m_constantBufferGS->Map(0, &readRange, reinterpret_cast<void**>(&m_pConstantBufferGSData)));
		ZeroMemory(m_pConstantBufferGSData, constantBufferGSSize);
	}

	m_hueSpectrum.reset(new Spectrum(*m_device.Get(), *m_commandList.Get(), 256));
	m_hueSpectrum->HueSpectrum(300.0f);
	m_hueSpectrum->CreateView(*m_srvHeap.Get(), SpectrumMap, m_srvUavDescriptorSize);

	// Create normal map texture
	{
		UINT texSize = 64;
		float halfSize = (float)texSize / 2.0f;
		std::vector<XMFLOAT3> texture;
		texture.reserve(texSize * texSize);

		for (size_t y = 0; y < texSize; ++y)
		{
			for (size_t x = 0; x < texSize; ++x)
			{
				XMFLOAT3 position;
				position.x = ((float)x - halfSize) / halfSize;
				position.y = (halfSize - (float)y) / halfSize;
				float zSq = 1 - (position.x * position.x + position.y * position.y);
				if (zSq >= 0.0f) {
					position.z = sqrtf(zSq);
				}
				else 
				{
					position.x = 0.0f;
					position.y = 0.0f;
					position.z = 0.0f;
				}
				texture.push_back(position);
			}
		}

		CD3DX12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32_FLOAT, texSize, texSize, 1, 1);

		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&m_particleNormal)
		));

		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_particleNormal.Get(), 0, 1) + D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;

		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_particleNormalUpload)));

		D3D12_SUBRESOURCE_DATA textureData = {};
		textureData.pData = reinterpret_cast<UINT8*>(texture.data());
		textureData.RowPitch = texSize * sizeof(XMFLOAT3);
		textureData.SlicePitch = textureData.RowPitch * texSize;

		UpdateSubresources<1>(m_commandList.Get(), m_particleNormal.Get(), m_particleNormalUpload.Get(), 0, 0, 1, &textureData);
		NAME_D3D12_OBJECT(m_particleNormal);

		CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(m_srvHeap->GetCPUDescriptorHandleForHeapStart(), NormalMap, m_srvUavDescriptorSize);
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = textureDesc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = textureDesc.MipLevels;
		m_device->CreateShaderResourceView(m_particleNormal.Get(), &srvDesc, cpuHandle);
	}

	m_parameters.reset(new SimulationParameters(*m_device.Get(), *m_commandList.Get(), 1.0f, ParticleCount));
	m_simulationState.reset(new SimulationStep(*m_device.Get(), *m_commandList.Get(), m_dataFrames, *m_parameters, GetAssetFullPath(L"SimulationStep.cso")));

	// Close the command list and execute it to begin the initial GPU setup.
	ThrowIfFailed(m_commandList->Close());
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Create synchronization objects and wait until assets have been uploaded to the GPU.
	{
		ThrowIfFailed(m_device->CreateFence(m_renderContextFenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_renderContextFence)));
		m_renderContextFenceValue++;

		m_renderContextFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (m_renderContextFenceEvent == nullptr)
		{
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
		}

		WaitForRenderContext();
	}
}

// Create the particle vertex buffer.
void D3D12nBodyGravity::CreateVertexBuffer()
{
	std::vector<ParticleVertex> vertices;
	vertices.resize(ParticleCount);
	for (UINT i = 0; i < ParticleCount; i++)
	{
		vertices[i].color = XMFLOAT4(1.0f, 1.0f, 0.2f, 1.0f);
	}
	const UINT bufferSize = ParticleCount * sizeof(ParticleVertex);

	ThrowIfFailed(m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&m_vertexBuffer)));

	ThrowIfFailed(m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_vertexBufferUpload)));

	NAME_D3D12_OBJECT(m_vertexBuffer);

	D3D12_SUBRESOURCE_DATA vertexData = {};
	vertexData.pData = reinterpret_cast<UINT8*>(&vertices[0]);
	vertexData.RowPitch = bufferSize;
	vertexData.SlicePitch = vertexData.RowPitch;

	UpdateSubresources<1>(m_commandList.Get(), m_vertexBuffer.Get(), m_vertexBufferUpload.Get(), 0, 0, 1, &vertexData);
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_vertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

	m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
	m_vertexBufferView.SizeInBytes = static_cast<UINT>(bufferSize);
	m_vertexBufferView.StrideInBytes = sizeof(ParticleVertex);
}

// Create the position and velocity buffer shader resources.
void D3D12nBodyGravity::CreateParticleBuffers()
{
	m_dataFrames[0].m_pointList.reset(new PointList(*m_device.Get(), *m_commandList.Get(), ParticleCount, ParticleSpread));
	m_dataFrames[1].m_pointList.reset(new PointList(*m_dataFrames[0].m_pointList));

	UINT32 indexResolution = 32;

	m_dataFrames[0].m_spacialIndex.reset(new SpacialIndex(
		*m_device.Get(),
		*m_commandList.Get(), 
		ParticleSpread * 2.0f / indexResolution,
		indexResolution
	));

	m_dataFrames[1].m_spacialIndex.reset(new SpacialIndex(
		*m_device.Get(),
		*m_commandList.Get(),
		ParticleSpread * 2.0f / indexResolution,
		indexResolution
	));

	m_dataFrames[0].PopulateIndex();
	m_dataFrames[1].PopulateIndex();

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = ParticleCount;
	srvDesc.Buffer.StructureByteStride = sizeof(PointList::Particle);
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

		
	CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle0(m_srvUavHeap->GetCPUDescriptorHandleForHeapStart(), SrvParticlePosVelo0, m_srvUavDescriptorSize);
	CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle1(m_srvUavHeap->GetCPUDescriptorHandleForHeapStart(), SrvParticlePosVelo1, m_srvUavDescriptorSize);
	m_device->CreateShaderResourceView(m_dataFrames[0].m_pointList->GetResource(), &srvDesc, srvHandle0);
	m_device->CreateShaderResourceView(m_dataFrames[1].m_pointList->GetResource(), &srvDesc, srvHandle1);

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = ParticleCount;
	uavDesc.Buffer.StructureByteStride = sizeof(PointList::Particle);
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle0(m_srvUavHeap->GetCPUDescriptorHandleForHeapStart(), UavParticlePosVelo0, m_srvUavDescriptorSize);
	CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle1(m_srvUavHeap->GetCPUDescriptorHandleForHeapStart(), UavParticlePosVelo1, m_srvUavDescriptorSize);
	m_device->CreateUnorderedAccessView(m_dataFrames[0].m_pointList->GetResource(), nullptr, &uavDesc, uavHandle0);
	m_device->CreateUnorderedAccessView(m_dataFrames[1].m_pointList->GetResource(), nullptr, &uavDesc, uavHandle1);
}

void D3D12nBodyGravity::CreateAsyncContexts()
{
	// Create compute resources.
	D3D12_COMMAND_QUEUE_DESC queueDesc = { D3D12_COMMAND_LIST_TYPE_COMPUTE, 0, D3D12_COMMAND_QUEUE_FLAG_NONE };
	ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_computeCommandQueue)));
	ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&m_computeAllocator)));
	ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, m_computeAllocator.Get(), nullptr, IID_PPV_ARGS(&m_computeCommandList)));
	ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&m_threadFences)));

	m_threadFenceEvents = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (m_threadFenceEvents == nullptr)
	{
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}

	m_threadData.pContext = this;
	m_threadData.threadIndex = 0;

	m_threadHandles = CreateThread(
		nullptr,
		0,
		reinterpret_cast<LPTHREAD_START_ROUTINE>(ThreadProc),
		reinterpret_cast<void*>(&m_threadData),
		CREATE_SUSPENDED,
		nullptr);

	ResumeThread(m_threadHandles);
}

// Update frame-based values.
void D3D12nBodyGravity::OnUpdate()
{
	// Wait for the previous Present to complete.
	WaitForSingleObjectEx(m_swapChainEvent, 100, FALSE);

	m_timer.Tick(NULL);
	m_camera.Update(static_cast<float>(m_timer.GetElapsedSeconds()));

	ConstantBufferGS constantBufferGS = {};
	XMStoreFloat4x4(&constantBufferGS.worldViewProjection, XMMatrixMultiply(m_camera.GetViewMatrix(), m_camera.GetProjectionMatrix(0.8f, m_aspectRatio, 1.0f, 5000.0f)));
	XMStoreFloat4x4(&constantBufferGS.inverseView, XMMatrixInverse(nullptr, m_camera.GetViewMatrix()));

	UINT8* destination = m_pConstantBufferGSData + sizeof(ConstantBufferGS) * m_frameIndex;
	memcpy(destination, &constantBufferGS, sizeof(ConstantBufferGS));
}

// Render the scene.
void D3D12nBodyGravity::OnRender()
{
	InterlockedExchange(&m_renderContextFenceValues, m_renderContextFenceValue);

	UINT64 threadFenceValue = InterlockedGetValue(&m_threadFenceValues);
	if (m_threadFences->GetCompletedValue() < threadFenceValue)
	{
		// Instruct the rendering command queue to wait for the current 
		// compute work to complete.
		ThrowIfFailed(m_commandQueue->Wait(m_threadFences.Get(), threadFenceValue));
	}

	PIXBeginEvent(m_commandQueue.Get(), 0, L"Render");

	// Record all the commands we need to render the scene into the command list.
	PopulateCommandList();

	// Execute the command list.
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	PIXEndEvent(m_commandQueue.Get());

	// Present the frame.
	ThrowIfFailed(m_swapChain->Present(1, 0));

	MoveToNextFrame();
}

// Fill the command list with all the render commands and dependent state.
void D3D12nBodyGravity::PopulateCommandList()
{
	// Command list allocators can only be reset when the associated
	// command lists have finished execution on the GPU; apps should use
	// fences to determine GPU execution progress.
	ThrowIfFailed(m_commandAllocators[m_frameIndex]->Reset());

	// However, when ExecuteCommandList() is called on a particular command
	// list, that command list can then be reset at any time and must be before
	// re-recording.
	ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), m_pipelineState.Get()));

	// Set necessary state.
	m_commandList->SetPipelineState(m_pipelineState.Get());
	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

	m_commandList->SetGraphicsRootConstantBufferView(GraphicsRootCBV, m_constantBufferGS->GetGPUVirtualAddress() + m_frameIndex * sizeof(ConstantBufferGS));

	ID3D12DescriptorHeap* pixelHeaps[] = { m_srvHeap.Get() };
	m_commandList->SetDescriptorHeaps(_countof(pixelHeaps), pixelHeaps);

	CD3DX12_GPU_DESCRIPTOR_HANDLE srvPixelHandle(m_srvHeap->GetGPUDescriptorHandleForHeapStart(), NormalMap, m_srvUavDescriptorSize);
	m_commandList->SetGraphicsRootDescriptorTable(GraphicsRootPixelTable, srvPixelHandle);

	ID3D12DescriptorHeap* ppHeaps[] = { m_srvUavHeap.Get() };
	m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	// Indicate that the back buffer will be used as a render target.
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart(), 0, m_dsvDescriptorSize);
	m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	// Record commands.
	const float clearColor[] = { 0.0f, 0.1f, 0.2f, 0.0f };
	m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	// Render the particles.
	float viewportHeight = static_cast<float>(static_cast<UINT>(m_viewport.Height) / m_heightInstances);
	float viewportWidth = static_cast<float>(static_cast<UINT>(m_viewport.Width) / m_widthInstances);

	const UINT srvIndex = ((m_parameters->GetData().currentBatch & 0x1) == 0 ? SrvParticlePosVelo0 : SrvParticlePosVelo1);

	CD3DX12_VIEWPORT viewport(
		0.0f,
		0.0f,
		viewportWidth,
		viewportHeight);

	m_commandList->RSSetViewports(1, &viewport);

	CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle(m_srvUavHeap->GetGPUDescriptorHandleForHeapStart(), srvIndex, m_srvUavDescriptorSize);
	m_commandList->SetGraphicsRootDescriptorTable(GraphicsRootSRVTable, srvHandle);

	PIXBeginEvent(m_commandList.Get(), 0, L"Draw particles for thread %u", 0);
	m_commandList->DrawInstanced(ParticleCount, 1, 0, 0);
	PIXEndEvent(m_commandList.Get());

	m_commandList->RSSetViewports(1, &m_viewport);

	// Indicate that the back buffer will now be used to present.
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed(m_commandList->Close());
}

DWORD D3D12nBodyGravity::AsyncComputeThreadProc(int threadIndex)
{
	ID3D12CommandQueue* pCommandQueue = m_computeCommandQueue.Get();
	ID3D12CommandAllocator* pCommandAllocator = m_computeAllocator.Get();
	ID3D12GraphicsCommandList* pCommandList = m_computeCommandList.Get();
	ID3D12Fence* pFence = m_threadFences.Get();

	while (0 == InterlockedGetValue(&m_terminating))
	{
		// Run the particle simulation.
		Simulate(0);

		// Close and execute the command list.
		ThrowIfFailed(pCommandList->Close());
		ID3D12CommandList* ppCommandLists[] = { pCommandList };

		PIXBeginEvent(pCommandQueue, 0, L"Thread %d: Iterate on the particle simulation", 0);
		pCommandQueue->ExecuteCommandLists(1, ppCommandLists);
		PIXEndEvent(pCommandQueue);

		// Wait for the compute shader to complete the simulation.
		UINT64 threadFenceValue = InterlockedIncrement(&m_threadFenceValues);
		ThrowIfFailed(pCommandQueue->Signal(pFence, threadFenceValue));
		ThrowIfFailed(pFence->SetEventOnCompletion(threadFenceValue, m_threadFenceEvents));
		WaitForSingleObject(m_threadFenceEvents, INFINITE);


		// Wait for the render thread to be done with the SRV so that
		// the next frame in the simulation can run.
		UINT64 renderContextFenceValue = InterlockedGetValue(&m_renderContextFenceValues);
		if (m_renderContextFence->GetCompletedValue() < renderContextFenceValue)
		{
			ThrowIfFailed(pCommandQueue->Wait(m_renderContextFence.Get(), renderContextFenceValue));
			InterlockedExchange(&m_renderContextFenceValues, 0);
		}

		m_parameters->PostUpdate();

		// Prepare for the next frame.
		ThrowIfFailed(pCommandAllocator->Reset());
		m_simulationState->Reset(*pCommandList, *pCommandAllocator);
	}

	return 0;
}

// Run the particle simulation using the compute shader.
void D3D12nBodyGravity::Simulate(UINT threadIndex)
{
	ID3D12GraphicsCommandList* pCommandList = m_computeCommandList.Get();
	m_simulationState->Step(m_parameters->GetData().currentBatch & 0x1, *pCommandList);
}

void D3D12nBodyGravity::OnDestroy()
{
	// Notify the compute threads that the app is shutting down.
	InterlockedExchange(&m_terminating, 1);
	WaitForMultipleObjects(1, &m_threadHandles, TRUE, INFINITE);

	// Ensure that the GPU is no longer referencing resources that are about to be
	// cleaned up by the destructor.
	WaitForRenderContext();

	// Close handles to fence events and threads.
	CloseHandle(m_renderContextFenceEvent);
	CloseHandle(m_threadHandles);
	CloseHandle(m_threadFenceEvents);
}

void D3D12nBodyGravity::OnKeyDown(UINT8 key)
{
	m_camera.OnKeyDown(key);
}

void D3D12nBodyGravity::OnKeyUp(UINT8 key)
{
	m_camera.OnKeyUp(key);
}

void D3D12nBodyGravity::WaitForRenderContext()
{
	// Add a signal command to the queue.
	ThrowIfFailed(m_commandQueue->Signal(m_renderContextFence.Get(), m_renderContextFenceValue));

	// Instruct the fence to set the event object when the signal command completes.
	ThrowIfFailed(m_renderContextFence->SetEventOnCompletion(m_renderContextFenceValue, m_renderContextFenceEvent));
	m_renderContextFenceValue++;

	// Wait until the signal command has been processed.
	WaitForSingleObject(m_renderContextFenceEvent, INFINITE);
}

// Cycle through the frame resources. This method blocks execution if the 
// next frame resource in the queue has not yet had its previous contents 
// processed by the GPU.
void D3D12nBodyGravity::MoveToNextFrame()
{
	// Assign the current fence value to the current frame.
	m_frameFenceValues[m_frameIndex] = m_renderContextFenceValue;

	// Signal and increment the fence value.
	ThrowIfFailed(m_commandQueue->Signal(m_renderContextFence.Get(), m_renderContextFenceValue));
	m_renderContextFenceValue++;

	// Update the frame index.
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	// If the next frame is not ready to be rendered yet, wait until it is ready.
	if (m_renderContextFence->GetCompletedValue() < m_frameFenceValues[m_frameIndex])
	{
		ThrowIfFailed(m_renderContextFence->SetEventOnCompletion(m_frameFenceValues[m_frameIndex], m_renderContextFenceEvent));
		WaitForSingleObject(m_renderContextFenceEvent, INFINITE);
	}
}
