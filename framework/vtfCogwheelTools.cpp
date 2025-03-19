#include "vtfCogwheelTools.hpp"
#include "vtfMatrix.hpp"

#include <complex>
#include <cmath>

namespace vtf
{

Vec2 involute (float baseRadius, float theta, bool ccw = false, float angleOffset = 0.0f)
{
	const float dir = ccw ? -1.0f : +1.0f;
	const float angle = theta + angleOffset;
	const float x = baseRadius * (std::cos(angle) + dir * angle * std::sin(angle));
	const float y = baseRadius * (std::sin(angle) - dir * angle * std::cos(angle));
	return { x, y };
}

Vec2 circle (float radius, float theta, float cx = 0.0f, float cy = 0.0f)
{
	const float x = cx + radius * std::cos(theta);
	const float y = cy + radius * std::sin(theta);
	return { x, y };
}

Vec2 rotate (add_cref<Vec2> p, float angle)
{
	const float x = p.x() * std::cos(angle) - p.y() * std::sin(angle);
	const float y = p.x() * std::sin(angle) + p.y() * std::cos(angle);
	return { x, y };
}

float distance (add_cref<Vec2> a, add_cref<Vec2> b)
{
	const float x = a.x() - b.x();
	const float y = a.y() - b.y();
	return std::sqrt(x * x + y * y);
}

// dlugosc luku liczona ze wzoru: arc_length = radius * angle_in_radians

//float baseRadius = (innerDiameter / 2.0f) * cos(pressureAngle * pi / 180.0f);
//float pitchRadius = module * teethCount / 2.0f;

// tooth thickness & space width & backslash [0.1-0.2; 0.25-0.4; ~1]%
// tooth (space | gap | root), gear tooth profile
// addendum circle diameter (D_a)
// pitch circle diameter (PCD)
// dedendum circle diameter (root diameter) (D_f)

std::vector<Vec2> makeCogwheel (add_cref<CogwheelDescription> cd, VkPrimitiveTopology topology)
{
	UNREF(topology);

	std::vector<Vec4> t;
	const float baseRadius = cd.D / 2.0f;
	const float addenRadius = baseRadius + cd.Ha;
	const float dedenRadius = baseRadius - cd.Hf;

	const float toothAndGapAngle = glm::two_pi<float>() / float(cd.Z);
	const float addenToothAndGapArcLength = (glm::two_pi<float>() * addenRadius) / float(cd.Z);
	const float addenToothArcLength = addenToothAndGapArcLength / 2.0f;
	//const float addenToothArcAngle = addenToothArcLength / addenRadius;

	const float angleStep = 0.01f;
	float phi = 0.0;

	// punkt na kole podstaw
	t.push_back(circle(dedenRadius, 0.0f).cast<Vec4>(dedenRadius, 0.0f));

	// punkty lewej strony ewolwenty
	//const uint32_t risingBeginCount = data_count(t);
	{
		float theta = 0.0f;
		float r = 0.0f;
		while (r < addenRadius)
		{
			const Vec2 ip = involute(baseRadius, theta);
			const float prevPhi = phi;
			phi = std::arg(std::complex<float>(ip.x(), ip.y()));
			r = distance(ip, {});
			t.emplace_back(ip.cast<Vec4>(r, std::abs(phi - prevPhi)));
			theta += angleStep;
		}
	}
	const uint32_t risingEndCount = data_count(t);

	// dlugosc luku glowy zeba na kole glow
	const float addenToothTopArcLength = std::max(addenToothArcLength - (phi * addenRadius * 2.0f), 0.0f);

	// punkty glowy zeba
	float arcLength = phi * addenRadius;
	const float arcLengthMax = arcLength + addenToothTopArcLength;
	while (arcLength < arcLengthMax)
	{
		const Vec2 p = circle(addenRadius, phi);
		t.emplace_back(p.cast<Vec4>(phi, angleStep));
		phi += angleStep;
		arcLength = phi * addenRadius;
	}

	// punkty prawej strony ewolwenty (odbicie lustrzane lewej strony)
	for (uint32_t i = 0u; i < risingEndCount; ++i)
	{
		add_cref<Vec4> src = t[risingEndCount - i - 1u];
		const float diffPhi = src.w();
		const Vec2 p = circle(src.z(), phi);
		t.emplace_back(p.cast<Vec4>(0.0f, 0.0f));
		phi += diffPhi;
	}

	// punkty wrebu zeba na kole podstaw
	arcLength = phi * addenRadius;
	while (arcLength < addenToothAndGapArcLength)
	{
		const Vec2 p = circle(dedenRadius, phi);
		t.emplace_back(p.cast<Vec4>(phi, angleStep));
		phi += angleStep;
		arcLength = phi * addenRadius;
	}

	std::vector<Vec2> out;

	for (uint32_t i = 0u; i < cd.Z; ++i)
	{
		std::transform(t.begin(), t.end(), std::back_inserter(out), 
			[&](add_cref<Vec4> v) { return rotate(v.cast<Vec2>(), float(i) * toothAndGapAngle); });
	}

	return out;
}

} // namespace vtf
