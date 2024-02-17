#pragma once
#include "pch.h"

inline Config* config = nullptr;

inline std::optional<float> GetQualityOverrideRatio(const NVSDK_NGX_PerfQuality_Value input)
{
	if (config == nullptr || config == NULL)
		config = new Config(L"nvngx.ini");

	std::optional<float> output;

	if (!(config->QualityRatioOverrideEnabled.has_value() && config->QualityRatioOverrideEnabled))
		return output; // override not enabled

	switch (input)
	{
	case NVSDK_NGX_PerfQuality_Value_UltraPerformance:
		output = config->QualityRatio_UltraPerformance;
		break;
	case NVSDK_NGX_PerfQuality_Value_MaxPerf:
		output = config->QualityRatio_Performance;
		break;
	case NVSDK_NGX_PerfQuality_Value_Balanced:
		output = config->QualityRatio_Balanced;
		break;
	case NVSDK_NGX_PerfQuality_Value_MaxQuality:
		output = config->QualityRatio_Quality;
		break;
	case NVSDK_NGX_PerfQuality_Value_UltraQuality:
		output = config->QualityRatio_UltraQuality;
		break;
	default:
		spdlog::warn("GetQualityOverrideRatio: Unknown quality: {0}", (int)input);
		output = config->QualityRatio_Balanced;
		break;
	}
	return output;
}

inline NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_DLSS_GetOptimalSettingsCallback(NVSDK_NGX_Parameter* InParams)
{
	unsigned int Width;
	unsigned int Height;
	unsigned int OutWidth;
	unsigned int OutHeight;
	int PerfQualityValue;

	if (InParams->Get(NVSDK_NGX_Parameter_Width, &Width) != NVSDK_NGX_Result_Success)
		return NVSDK_NGX_Result_Fail;

	if (InParams->Get(NVSDK_NGX_Parameter_Height, &Height) != NVSDK_NGX_Result_Success)
		return NVSDK_NGX_Result_Fail;

	if (InParams->Get(NVSDK_NGX_Parameter_PerfQualityValue, &PerfQualityValue) != NVSDK_NGX_Result_Success)
		return NVSDK_NGX_Result_Fail;

	auto enumPQValue = (NVSDK_NGX_PerfQuality_Value)PerfQualityValue;

	spdlog::debug("NVSDK_NGX_DLSS_GetOptimalSettingsCallback start: in-> {0}x{1}", Width, Height);

	const std::optional<float> QualityRatio = GetQualityOverrideRatio(enumPQValue);

	if (QualityRatio.has_value()) {
		OutHeight = (unsigned int)((float)Height / QualityRatio.value());
		OutWidth = (unsigned int)((float)Width / QualityRatio.value());
	}
	else {
		spdlog::debug("NVSDK_NGX_DLSS_GetOptimalSettingsCallback Quality: {0}", PerfQualityValue);

		switch (enumPQValue)
		{
		case NVSDK_NGX_PerfQuality_Value_UltraPerformance:
			OutHeight = (unsigned int)((float)Height / 3.0);
			OutWidth = (unsigned int)((float)Width / 3.0);
			break;
		case NVSDK_NGX_PerfQuality_Value_MaxPerf:
			OutHeight = (unsigned int)((float)Height / 2.0);
			OutWidth = (unsigned int)((float)Width / 2.0);
			break;
		case NVSDK_NGX_PerfQuality_Value_Balanced:
			OutHeight = (unsigned int)((float)Height / 1.699115044247788);
			OutWidth = (unsigned int)((float)Width / 1.699115044247788);
			break;
		case NVSDK_NGX_PerfQuality_Value_MaxQuality:
			OutHeight = (unsigned int)((float)Height / 1.5);
			OutWidth = (unsigned int)((float)Width / 1.5);
			break;
		case NVSDK_NGX_PerfQuality_Value_UltraQuality:
			OutHeight = (unsigned int)((float)Height / 1.299932295192959);
			OutWidth = (unsigned int)((float)Width / 1.299932295192959);
			break;
		default:
			OutHeight = (unsigned int)((float)Height / 1.699115044247788);
			OutWidth = (unsigned int)((float)Width / 1.699115044247788);
			break;
		}
	}

	InParams->Set(NVSDK_NGX_Parameter_OutWidth, OutWidth);
	InParams->Set(NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Min_Render_Width, OutWidth);
	InParams->Set(NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Max_Render_Width, Width);
	InParams->Set(NVSDK_NGX_Parameter_OutHeight, OutHeight);
	InParams->Set(NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Min_Render_Height, OutHeight);
	InParams->Set(NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Max_Render_Height, Height);
	InParams->Set(NVSDK_NGX_Parameter_SizeInBytes, 1920 * 1080 * 31);

	spdlog::debug("EvaluateRenderScale end: in-> {0}x{1} out-> {2}x{3}", Width, Height, OutWidth, OutHeight);
	return NVSDK_NGX_Result_Success;
}

inline NVSDK_NGX_Result NVSDK_CONV NVSDK_NGX_DLSS_GetStatsCallback(NVSDK_NGX_Parameter* InParams)
{
	spdlog::debug("NVSDK_NGX_DLSS_GetStatsCallback");

	unsigned int Width;
	unsigned int Height;

	if (InParams->Get(NVSDK_NGX_Parameter_Width, &Width) != NVSDK_NGX_Result_Success && InParams->Get(NVSDK_NGX_Parameter_Height, &Height) != NVSDK_NGX_Result_Success)
		InParams->Set(NVSDK_NGX_Parameter_SizeInBytes, Width * Height * 31);
	else
		InParams->Set(NVSDK_NGX_Parameter_SizeInBytes, 1920 * 1080 * 31);
	
	return NVSDK_NGX_Result_Success;
}

struct Parameter
{
	template<typename T>
	void operator=(T value)
	{
		key = typeid(T).hash_code();
		if constexpr (std::is_same<T, float>::value) values.f = value;
		else if constexpr (std::is_same<T, int>::value) values.i = value;
		else if constexpr (std::is_same<T, unsigned int>::value) values.ui = value;
		else if constexpr (std::is_same<T, double>::value) values.d = value;
		else if constexpr (std::is_same<T, unsigned long long>::value) values.ull = value;
		else if constexpr (std::is_same<T, ID3D11Resource*>::value) values.vp = value;
		else if constexpr (std::is_same<T, ID3D12Resource*>::value) values.vp = value;
		else if constexpr (std::is_same<T, void*>::value) values.vp = value;
	}

	template<typename T>
	operator T() const
	{
		T v = {};
		if constexpr (std::is_same<T, float>::value)
		{
			if (key == typeid(unsigned long long).hash_code()) v = (T)values.ull;
			else if (key == typeid(float).hash_code()) v = (T)values.f;
			else if (key == typeid(double).hash_code()) v = (T)values.d;
			else if (key == typeid(unsigned int).hash_code()) v = (T)values.ui;
			else if (key == typeid(int).hash_code()) v = (T)values.i;
		}
		else if constexpr (std::is_same<T, int>::value)
		{
			if (key == typeid(unsigned long long).hash_code()) v = (T)values.ull;
			else if (key == typeid(float).hash_code()) v = (T)values.f;
			else if (key == typeid(double).hash_code()) v = (T)values.d;
			else if (key == typeid(int).hash_code()) v = (T)values.i;
			else if (key == typeid(unsigned int).hash_code()) v = (T)values.ui;
		}
		else if constexpr (std::is_same<T, unsigned int>::value)
		{
			if (key == typeid(unsigned long long).hash_code()) v = (T)values.ull;
			else if (key == typeid(float).hash_code()) v = (T)values.f;
			else if (key == typeid(double).hash_code()) v = (T)values.d;
			else if (key == typeid(int).hash_code()) v = (T)values.i;
			else if (key == typeid(unsigned int).hash_code()) v = (T)values.ui;
		}
		else if constexpr (std::is_same<T, double>::value)
		{
			if (key == typeid(unsigned long long).hash_code()) v = (T)values.ull;
			else if (key == typeid(float).hash_code()) v = (T)values.f;
			else if (key == typeid(double).hash_code()) v = (T)values.d;
			else if (key == typeid(int).hash_code()) v = (T)values.i;
			else if (key == typeid(unsigned int).hash_code()) v = (T)values.ui;
		}
		else if constexpr (std::is_same<T, unsigned long long>::value)
		{
			if (key == typeid(unsigned long long).hash_code()) v = (T)values.ull;
			else if (key == typeid(float).hash_code()) v = (T)values.f;
			else if (key == typeid(double).hash_code()) v = (T)values.d;
			else if (key == typeid(int).hash_code()) v = (T)values.i;
			else if (key == typeid(unsigned int).hash_code()) v = (T)values.ui;
			else if (key == typeid(void*).hash_code()) v = (T)values.vp;
		}
		else if constexpr (std::is_same<T, void*>::value)
		{
			if (key == typeid(void*).hash_code()) v = values.vp;
		}
		else if constexpr (std::is_same<T, ID3D11Resource*>::value)
		{
			if (key == typeid(ID3D11Resource*).hash_code()) v = values.d11r;
		}
		else if constexpr (std::is_same<T, ID3D12Resource*>::value)
		{
			if (key == typeid(ID3D12Resource*).hash_code()) v = values.d12r;
		}

		return v;
	}

	union
	{
		float f;
		double d;
		int i;
		unsigned int ui;
		unsigned long long ull;
		void* vp;
		ID3D11Resource* d11r;
		ID3D12Resource* d12r;
	} values;

	size_t key = 0;
};

struct NGXParameters : public NVSDK_NGX_Parameter
{
	void Set(const char* key, unsigned long long value) override { spdlog::debug("NGXParameters::set ulong('{0}', {1})", key, value); setT(key, value); }
	void Set(const char* key, float value) override { spdlog::debug("NGXParameters::set float('{0}', {1})", key, value); setT(key, value); }
	void Set(const char* key, double value) override { spdlog::debug("NGXParameters::set double('{0}', {1})", key, value); setT(key, value); }
	void Set(const char* key, unsigned int value) override { spdlog::debug("NGXParameters::set uint('{0}', {1})", key, value); setT(key, value); }
	void Set(const char* key, int value) override { spdlog::debug("NGXParameters::set int('{0}', {1})", key, value); setT(key, value); }
	void Set(const char* key, void* value) override { spdlog::debug("NGXParameters::set void('{0}')", key); setT(key, value); }
	void Set(const char* key, ID3D11Resource* value) override { spdlog::debug("NGXParameters::set d3d11('{0}')", key); setT(key, value); }
	void Set(const char* key, ID3D12Resource* value) override { spdlog::debug("NGXParameters::set d3d12('{0}')", key); setT(key, value); }

	NVSDK_NGX_Result Get(const char* key, unsigned long long* value) const override { auto result = getT(key, value); if (result == NVSDK_NGX_Result_Success) { spdlog::debug("NGXParameters::get ulong('{0}', {1})", key, *value); return NVSDK_NGX_Result_Success; } return NVSDK_NGX_Result_Fail; }
	NVSDK_NGX_Result Get(const char* key, float* value) const override { auto result = getT(key, value); if (result == NVSDK_NGX_Result_Success) { spdlog::debug("NGXParameters::get float('{0}', {1})", key, *value); return NVSDK_NGX_Result_Success; } return NVSDK_NGX_Result_Fail; }
	NVSDK_NGX_Result Get(const char* key, double* value) const override { auto result = getT(key, value); if (result == NVSDK_NGX_Result_Success) { spdlog::debug("NGXParameters::get double('{0}', {1})", key, *value); return NVSDK_NGX_Result_Success; } return NVSDK_NGX_Result_Fail; }
	NVSDK_NGX_Result Get(const char* key, unsigned int* value) const override { auto result = getT(key, value); if (result == NVSDK_NGX_Result_Success) { spdlog::debug("NGXParameters::get uint('{0}', {1})", key, *value); return NVSDK_NGX_Result_Success; } return NVSDK_NGX_Result_Fail; }
	NVSDK_NGX_Result Get(const char* key, int* value) const override { auto result = getT(key, value); if (result == NVSDK_NGX_Result_Success) { spdlog::debug("NGXParameters::get int('{0}', {1})", key, *value); return NVSDK_NGX_Result_Success; } return NVSDK_NGX_Result_Fail; }
	NVSDK_NGX_Result Get(const char* key, void** value) const override { auto result = getT(key, value); if (result == NVSDK_NGX_Result_Success) { spdlog::debug("NGXParameters::get void('{0}')", key); return NVSDK_NGX_Result_Success; } return NVSDK_NGX_Result_Fail; }
	NVSDK_NGX_Result Get(const char* key, ID3D11Resource** value) const override { auto result = getT(key, value); if (result == NVSDK_NGX_Result_Success) { spdlog::debug("NGXParameters::get d3d11('{0}')", key); return NVSDK_NGX_Result_Success; } return NVSDK_NGX_Result_Fail; }
	NVSDK_NGX_Result Get(const char* key, ID3D12Resource** value) const override { auto result = getT(key, value); if (result == NVSDK_NGX_Result_Success) { spdlog::debug("NGXParameters::get d3d12('{0}')", key); return NVSDK_NGX_Result_Success; } return NVSDK_NGX_Result_Fail; }

	void Reset() override
	{
		if (!m_values.empty())
			m_values.clear();

		spdlog::debug("NGXParameters::Reset Start");

		Set(NVSDK_NGX_Parameter_SuperSampling_Available, 1);
		Set(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMajor, 400);
		Set(NVSDK_NGX_Parameter_SuperSampling_MinDriverVersionMinor, 0);
		Set(NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, 0);
		Set(NVSDK_NGX_Parameter_SuperSampling_FeatureInitResult, 1);
		Set(NVSDK_NGX_Parameter_OptLevel, 0);
		Set(NVSDK_NGX_Parameter_IsDevSnippetBranch, 0);
		Set(NVSDK_NGX_Parameter_DLSSOptimalSettingsCallback, NVSDK_NGX_DLSS_GetOptimalSettingsCallback);
		Set(NVSDK_NGX_Parameter_DLSSGetStatsCallback, NVSDK_NGX_DLSS_GetOptimalSettingsCallback);
		Set(NVSDK_NGX_Parameter_Sharpness, 0.0f);
		Set(NVSDK_NGX_Parameter_MV_Scale_X, 1.0f);
		Set(NVSDK_NGX_Parameter_MV_Scale_Y, 1.0f);
		Set(NVSDK_NGX_Parameter_MV_Offset_X, 0.0f);
		Set(NVSDK_NGX_Parameter_MV_Offset_Y, 0.0f);
		Set(NVSDK_NGX_Parameter_DLSS_Exposure_Scale, 1.0f);

		Set(NVSDK_NGX_EParameter_SuperSampling_Available, 1);
		Set(NVSDK_NGX_EParameter_OptLevel, 0);
		Set(NVSDK_NGX_EParameter_IsDevSnippetBranch, 0);
		Set(NVSDK_NGX_EParameter_DLSSOptimalSettingsCallback, NVSDK_NGX_DLSS_GetOptimalSettingsCallback);
		Set(NVSDK_NGX_EParameter_Sharpness, 0.0f);
		Set(NVSDK_NGX_EParameter_MV_Scale_X, 1.0f);
		Set(NVSDK_NGX_EParameter_MV_Scale_Y, 1.0f);
		Set(NVSDK_NGX_EParameter_MV_Offset_X, 0.0f);
		Set(NVSDK_NGX_EParameter_MV_Offset_Y, 0.0f);

		spdlog::debug("NGXParameters::Reset End");
	}

	std::vector<std::string> enumerate() const
	{
		std::vector<std::string> keys;
		for (auto& value : m_values)
		{
			keys.push_back(value.first);
		}
		return keys;
	}

private:
	std::map<std::string, Parameter> m_values;
	mutable std::mutex m_mutex;

	template<typename T>
	void setT(const char* key, T& value)
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		m_values[key] = value;
	}

	template<typename T>
	NVSDK_NGX_Result getT(const char* key, T* value) const
	{
		const std::lock_guard<std::mutex> lock(m_mutex);
		auto k = m_values.find(key);
		if (k == m_values.end())
		{
			spdlog::warn("NGXParameters::getT('{0}', FAIL)", key);
			return NVSDK_NGX_Result_Fail;
		};
		const Parameter& p = (*k).second;
		*value = p;

		return NVSDK_NGX_Result_Success;
	}


};

inline NVSDK_NGX_Parameter* GetNGXParameters()
{
	auto params = new NGXParameters();
	params->Reset();

	return params;
}
