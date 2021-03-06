#pragma once

#include <memory>
#include "PointList.h"
#include "SpacialIndex.h"

struct DataFrame
{
	std::unique_ptr<PointList> m_pointList;
	std::unique_ptr<SpacialIndex> m_spacialIndex;

	void PopulateIndex(ID3D12GraphicsCommandList& commandList);

	DataFrame();
private:
	DataFrame(const DataFrame& other);
	void operator=(const DataFrame& other);
};

