#pragma once

#include "PostEffect.h"

class BloomEffect : public PostEffect {
public:
	void Init(unsigned width, unsigned height) override;

	void ApplyEffect(PostEffect* buffer) override;

	void Reshape(unsigned width, unsigned height) override;

	float GetDownscale() const;
	float GetThreshold() const;
	unsigned GetPasses() const;

	void SetDownscale(float downscale);
	void SetThreshold(float threshold);
	void SetPasses(unsigned passes);

private:
	float _downscale = 2.f;
	float _threshold = 0.01f;
	unsigned _passes = 10;
	glm::vec2 _pixelSize;

};