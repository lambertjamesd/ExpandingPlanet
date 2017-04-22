#pragma once

#include <memory>
#include "PointList.h"
#include "SpacialIndex.h"

struct DataFrame
{
	std::unique_ptr<PointList> m_pointList;
	std::unique_ptr<SpacialIndex> m_spacialIndex;
};

