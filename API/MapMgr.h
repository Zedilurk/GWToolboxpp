#pragma once

#include <Windows.h>
#include "GWAPIMgr.h"


namespace GWAPI{
	class MapMgr{
		GWAPIMgr* parent;

		struct PAB_ZoneMap {
			const DWORD header = 0xAB;
			DWORD mapid;
			int region;
			int district;
			int language;
			DWORD unk;
		};
	public:

		MapMgr(GWAPIMgr* obj);

		GwConstants::MapID GetMapID();

		inline int GetRegion() { return *(int*)(MemoryMgr::MapInfoPtr + 0x10); }
		inline int GetLanguage() { return *(int*)(MemoryMgr::MapInfoPtr + 0xC); }

		DWORD GetInstanceTime();

		// Get the instance type (Outpost, Explorable or Loading)
		GwConstants::InstanceType GetInstanceType();

		void Travel(GwConstants::MapID MapID, DWORD District = 0, int Region = 0, DWORD Language = 0);


	};
}