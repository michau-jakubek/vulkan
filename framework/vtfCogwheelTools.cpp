#include "vtfCogwheelTools.hpp"
#include "vtfMatrix.hpp"

#include <complex>
#include <cmath>

namespace vtf
{

Vec2 involute (float baseRadius, float theta, bool ccw, float angleOffset)
{
	const float dir = ccw ? -1.0f : +1.0f;
	const float angle = theta + angleOffset;
	const float x = baseRadius * (std::cos(angle) + dir * angle * std::sin(angle));
	const float y = baseRadius * (std::sin(angle) - dir * angle * std::cos(angle));
	return { x, y };
}

Vec2 circle (float radius, float theta, float cx, float cy)
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

std::vector<Vec2> generateCogwheelOutlinePoints (add_cref<CogwheelDescription> cd)
{
	std::vector<Vec4> t;
	const float baseRadius = cd.mainDiameter / 2.0f;
	const float addenRadius = baseRadius + cd.toothHeadHeight;
	const float dedenRadius = baseRadius - cd.toothFootHeight;

	const float toothAndGapAngle = glm::two_pi<float>() / float(cd.toothCount);
	const float addenToothAndGapArcLength = toothAndGapAngle * addenRadius;
	const float addenToothArcLength = addenToothAndGapArcLength * cd.toothToSpaceFactor;
	const bool hasAxis = cd.wholeDiameter > 0.0f;

	const float angleStep = cd.angleStep;
	float phi = 0.0;

	if (hasAxis)
	{
		//t.push_back(circle(cd.wholeDiameter, 0.0f).cast<Vec4>(cd.wholeDiameter, 0.0f));
	}

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
	out.reserve(t.size() * cd.toothCount);

	for (uint32_t i = 0u; i < cd.toothCount; ++i)
	{
		std::transform(t.begin(), t.end(), std::back_inserter(out),
			[&](add_cref<Vec4> v) { return rotate(v.cast<Vec2>(), float(i) * toothAndGapAngle); });
	}

	return out;
}

std::vector<Vec4> generateCogwheelPrimitives (add_cref<std::vector<Vec2>> outlinePoints, float componentZ,
												VkPrimitiveTopology topology, VkFrontFace ff)
{
	if (VK_PRIMITIVE_TOPOLOGY_POINT_LIST == topology || VK_PRIMITIVE_TOPOLOGY_LINE_STRIP == topology)
	{
		std::vector<Vec4> v(outlinePoints.size());
		std::transform(outlinePoints.begin(), outlinePoints.end(), v.begin(),
			[&](add_cref<Vec2> u) { return Vec4(u.x(), u.y(), componentZ, 1.0f); });
		return v;
	}
	else if (VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN == topology)
	{
		std::vector<Vec4> v(outlinePoints.size() + 1u);
		v[0] = Vec4(0.0f, 0.0f, componentZ, 1.0f);
		std::transform(outlinePoints.begin(), outlinePoints.end(), std::next(v.begin()),
			[&](add_cref<Vec2> u) { return Vec4(u.x(), u.y(), componentZ, 1.0f); });
		return v;
	}
	else if (VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST == topology)
	{
		const uint32_t pc = data_count(outlinePoints);
		const uint32_t pcMax = pc ? (pc - 1u) : 0u;
		std::vector<Vec4> v(pc * 3u);

		for (uint32_t i = 0u; i < pcMax; ++i)
		{
			v[i * 3u] = Vec4(0.0f, 0.0f, componentZ, 1.0f);
			if (VK_FRONT_FACE_CLOCKWISE == ff)
			{
				v[i * 3u + 1u] = Vec4(outlinePoints[i][0u], outlinePoints[i][1u], componentZ, 1.0f);
				v[i * 3u + 2u] = Vec4(outlinePoints[i + 1u][0u], outlinePoints[i + 1u][1u], componentZ, 1.0f);
			}
			else
			{
				v[i * 3u + 1u] = Vec4(outlinePoints[i + 1u][0u], outlinePoints[i + 1u][1u], componentZ, 1.0f);
				v[i * 3u + 2u] = Vec4(outlinePoints[i][0u], outlinePoints[i][1u], componentZ, 1.0f);
			}
		}
		if (pcMax)
		{
			v[pcMax * 3u] = Vec4(0.0f, 0.0f, componentZ, 1.0f);
			if (VK_FRONT_FACE_CLOCKWISE == ff)
			{
				v[pcMax * 3u + 1u] = Vec4(outlinePoints[pcMax][0u], outlinePoints[pcMax][1u], componentZ, 1.0f);
				v[pcMax * 3u + 2u] = Vec4(outlinePoints[0u][0u], outlinePoints[0u][1u], componentZ, 1.0f);
			}
			else
			{
				v[pcMax * 3u + 1u] = Vec4(outlinePoints[0u][0u], outlinePoints[0u][1u], componentZ, 1.0f);
				v[pcMax * 3u + 2u] = Vec4(outlinePoints[pcMax][0u], outlinePoints[pcMax][1u], componentZ, 1.0f);
			}
		}
		return v;
	}
	else
	{
		ASSERTFALSE("Unknown topology");
	}
	return {};
}


std::vector<Vec4> generateCogwheelToothSurface(add_cref<std::vector<Vec2>> outlinePoints, float frontZ, float backZ,
	VkPrimitiveTopology topology, VkFrontFace ff)
{
	if (VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST == topology)
	{
		const uint32_t pc = data_count(outlinePoints);
		const uint32_t pcMax = pc ? (pc - 1u) : 0u;
		std::vector<Vec4> v(pc * 6u);

		for (uint32_t i = 0u; i < pcMax; ++i)
		{
			if (VK_FRONT_FACE_CLOCKWISE == ff)
			{
				v[i * 6u] = Vec4(outlinePoints[i].x(), outlinePoints[i].y(), frontZ, 1.0f);
				v[i * 6u + 1u] = Vec4(outlinePoints[i].x(), outlinePoints[i].y(), backZ, 1.0f);
				v[i * 6u + 2u] = Vec4(outlinePoints[i + 1u].x(), outlinePoints[i + 1u].y(), backZ, 1.0f);
				v[i * 6u + 3u] = Vec4(outlinePoints[i + 1u].x(), outlinePoints[i + 1u].y(), backZ, 1.0f);
				v[i * 6u + 4u] = Vec4(outlinePoints[i + 1u].x(), outlinePoints[i + 1u].y(), frontZ, 1.0f);
				v[i * 6u + 5u] = Vec4(outlinePoints[i].x(), outlinePoints[i].y(), frontZ, 1.0f);
			}
		}
		if (pcMax)
		{
			if (VK_FRONT_FACE_CLOCKWISE == ff)
			{
				v[pcMax * 6u] = Vec4(outlinePoints[pcMax].x(), outlinePoints[pcMax].y(), frontZ, 1.0f);
				v[pcMax * 6u + 1u] = Vec4(outlinePoints[pcMax].x(), outlinePoints[pcMax].y(), backZ, 1.0f);
				v[pcMax * 6u + 2u] = Vec4(outlinePoints[0u].x(), outlinePoints[0u].y(), backZ, 1.0f);
				v[pcMax * 6u + 3u] = Vec4(outlinePoints[0u].x(), outlinePoints[0u].y(), backZ, 1.0f);
				v[pcMax * 6u + 4u] = Vec4(outlinePoints[0u].x(), outlinePoints[0u].y(), frontZ, 1.0f);
				v[pcMax * 6u + 5u] = Vec4(outlinePoints[pcMax].x(), outlinePoints[pcMax].y(), frontZ, 1.0f);
			}
		}
		return v;
	}
	else
	{
		ASSERTFALSE("Unknown topology");
	}
	return {};
}

} // namespace vtf
