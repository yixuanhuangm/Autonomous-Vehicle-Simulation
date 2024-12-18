﻿// ==========================================================================
// Copyright (C) 2018 - 2021 Beijing 51WORLD Digital Twin Technology Co., Ltd. 
// , and/or its licensors.  All rights reserved.
//
// The coded instructions, statements, computer programs, and/or related 
// material (collectively the "Data") in these files contain unpublished
// information proprietary to Beijing 51WORLD Digital Twin Technology Co., Ltd. 
// ("51WORLD") and/or its licensors,  which is protected by the People's 
// Republic of China and/or other countries copyright law and by 
// international treaties.
//
// The Data may not be disclosed or distributed to third parties or be
// copied or duplicated, in whole or in part, without the prior written
// consent of 51WORLD.
//
// The copyright notices in the Software and this entire statement,
// including the above license grant, this restriction and the following
// disclaimer, must be included in all copies of the Software, in whole
// or in part, and all derivative works of the Software, unless such copies
// or derivative works are solely in the form of machine-executable object
// code generated by a source language processor.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
// 51WORLD DOES NOT MAKE AND HEREBY DISCLAIMS ANY EXPRESS OR IMPLIED
// WARRANTIES INCLUDING, BUT NOT LIMITED TO, THE WARRANTIES OF
// NON-INFRINGEMENT, MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE,
// OR ARISING FROM A COURSE OF DEALING, USAGE, OR TRADE PRACTICE. IN NO
// EVENT WILL 51WORLD AND/OR ITS LICENSORS BE LIABLE FOR ANY LOST
// REVENUES, DATA, OR PROFITS, OR SPECIAL, DIRECT, INDIRECT, OR
// CONSEQUENTIAL DAMAGES, EVEN IF 51WORLD AND/OR ITS LICENSORS HAS
// BEEN ADVISED OF THE POSSIBILITY OR PROBABILITY OF SUCH DAMAGES.
// ==========================================================================
#pragma once

#ifdef BUILD_SIMONE_API
#if defined(WIN32) || defined(_WIN32)
#define SIMONE_API __declspec(dllexport)
#elif defined(__linux__) || defined(__linux)
#define SIMONE_API __attribute__((visibility("default")))
#endif
#else
#define SIMONE_API
#endif

#include <string>
#include "Service/SimOneIOStruct.h"

#ifdef __cplusplus
extern "C"
{
#endif

	namespace SimOneAPI{
		/*!
		初始化评价服务
		\li function:
		*	InitEvaluationService
		\li brief:
		*	Initialize Evaluation Service for autonomous driving algorithm
		*	Judge data and GPS data with be sent to evaluation service as default(category: judge, mainvehicle)
		@param[in]
		*	mainVehicleId: Id of the main vehicle
		@param[in]
		*	serviceIP: Evaluation service ip
		@param[in]
		*	port: BridgeIO server port
		@param[in]
		*	withMainVehicle: whether to send MainVehicle data to evaluation server as default
		@return
		*	Success or not
		*/
		SIMONE_API bool InitEvaluationService(const char* mainVehicleId, const char *serviceIP = "127.0.0.1", int port = 8078, bool withMainVehicle = true);

		/*!
		初始化评价服务,本地贮存数据
		\li function:
		*	InitEvaluationService
		\li brief:
		*	Initialize Evaluation Service for autonomous driving algorithm
		*	Judge data and GPS data with be sent to evaluation service as default(category: judge, mainvehicle)
		@param[in]
		*	mainVehicleId: Id of the main vehicle
		@param[in]
		*	withMainVehicle: whether to send MainVehicle data to evaluation server as default
		@return
		*	Success or not
		*/
		SIMONE_API bool InitEvaluationServiceWithLocalData(const char* mainVehicleId, bool withMainVehicle = true);

		/*!
		添加评价算法所需的各类信息(JSON格式)
		\li function:
		*	AddEvaluationRecord
		\li brief:
		*	Add Evaluation record for Evaluation algorithm
		@param[in]
		*	mainVehicleId: Id of the main vehicle
		@param[in]
		*	jsonString: record string in json format, can be generated by json dump.
			The data includes property category, timestamp and others, e.g.
		*	{"category": "signal", "timestamp": 1648363819335, "ACC": "on", "ACCSpeed": 20}
		
		@return
		*	Success or not
		*/
		SIMONE_API bool AddEvaluationRecord(const char* mainVehicleId, const char * jsonString);

		/*!
		保存评价算法所需的各类信息(JSON格式)
		\li function:
		*	SaveEvaluationRecord
		\li brief:
		*	Save Evaluation record for Evaluation algorithm

		@return
		*	Success or not
		*/
		SIMONE_API bool SaveEvaluationRecord();

		/*!
		场景判定事件回调
		\li function:
		*	SetJudgeEventCB
		\li brief:
		*	Register the callback func applying for scenario judge event
		@param[in]
		*	cb: scenario judge event callback function
		*	param[out]
		*	mainVehicleId: Id of the main vehicle
		*	judgeEventDetailInfo: detail info for scenario judge event.

		@return
		*	Success or not
		*/
		SIMONE_API bool SetJudgeEventCB(void(*cb)(const char* mainVehicleId, SimOne_Data_JudgeEvent *judgeEventDetailInfo));

	}
#ifdef __cplusplus
}
#endif
