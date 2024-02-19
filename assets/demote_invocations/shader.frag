#version 450

#extension GL_KHR_shader_subgroup_quad : require
#extension GL_KHR_shader_subgroup_ballot : require
#extension GL_KHR_shader_subgroup_shuffle : require
#extension GL_KHR_shader_subgroup_clustered : require
#extension GL_KHR_shader_subgroup_arithmetic : require
#extension GL_EXT_demote_to_helper_invocation : require

// Reserved markers
#define NUM_SUBGROUPS			0
#define SUBGROUP_SIZE			1
#define NON_HELPER_COUNT		2
#define HELPER_COUNT			3
#define UNIQUE_NUM_SUBGROUPS    4
#define UINT_MAX				0xFFFFFFFF
// Optional markers
#define DATA_1					16
#define DATA_2					17
#define DATA_3					18
#define DATA_4					19
#define DATA_5					20
#define DATA_6					21
#define DATA_7					22
#define DATA_8					23

struct PixelInfo
{
	uint entryIndex;
	uint fragmentID;
	uint coordX;
	uint coordY;
	uint subgroupInfo;
	uint derivativeX;
	uint derivativeY;
	uint swapHorz;
	uint swapDiag;
	uint swapVert;
	uint broadcastResult;
	uint broadcastValue;
	uint clusteredMin;
	uint clusteredMax;
	uint quadLeader;
	uint place1;
	uint place2;
	uint place3;
	uint data1;
	uint data2;
	uint data3;
	uint data4;
	uint data5;
	uint data6;
	uint data7;
	uint data8;
	uint pad0;
	uint pad1;
};
void initStructure(in out PixelInfo i)
{
	i.coordX			= 0;
	i.coordY			= 0;
	i.fragmentID		= 0;
	i.subgroupInfo		= 0;
	i.derivativeX		= 0;
	i.derivativeY		= 0;
	i.entryIndex		= 0;
	i.swapHorz			= 0;
	i.swapDiag			= 0;
	i.swapVert			= 0;
	i.broadcastResult	= 0;
	i.broadcastValue	= 0;
	i.clusteredMin		= 0;
	i.clusteredMax		= 0;
	i.quadLeader		= 0;
	i.place1			= 0;
	i.place2			= 0;
	i.place3			= 0;
	i.data1				= 0;
	i.data2				= 0;
	i.data3				= 0;
	i.data4				= 0;
	i.data5				= 0;
	i.data6				= 0;
	i.data7				= 0;
	i.data8				= 0;
	i.pad0				= 0;
	i.pad1				= 0;
}
struct QuadInfo
{
	PixelInfo	base;
	PixelInfo	right;
	PixelInfo	bottom;
	PixelInfo	diagonal;
};
struct QuadPoints
{
	uvec2	base;
	uvec2	right;
	uvec2	bottom;
	uvec2	diagonal;
};

layout(location = 0) out vec4 attachment;
layout(binding = 0) buffer OutputP { uint p[]; } outputP;						// width * height * primitiveStride + 48
layout(binding = 1) buffer InputQuads { QuadPoints qp[]; } inputQuads;			// 4
layout(binding = 2) buffer OutputQuads { QuadInfo qi[]; } outputQuads;			// 2
layout(binding = 3) buffer OutputDerivatives { uint d[]; } outputDerivatives;	// width * height * primitiveStride * drvSize
layout(binding = 4) buffer TestIfHelper { uint h[]; } testIfHelper;				// width * height * primitiveStride
layout(push_constant) uniform PC
{
	int		width;
	int		height;
	uint	drvSize;
	float	pointSize;
	uint	primitiveStride;
	int		flags;
#define PERFORM_DEMOTING_BIT		  0x01
	//	uint32_t mPerformDemoting	  : 1;
#define DEMOTE_WHOLE_QUAD_BIT		  0x02
	//	uint32_t mDemoteQuad		  : 1;
	//	uint32_t mValidQuad			  : 1;
#define USE_SIMULATE_HELPER			  0x08
	//  uint32_t mUseSimulateHelper   : 1;
#define USE_COLOR_GROUP				  0x10
	//  uint32_t mColorGroup		 :  1;
	//	uint32_t mPad				 : 28;
};

const vec4 rgb[] = { vec4(   0, 255,   0, 1)	// Green
                   , vec4(   0,   0, 255, 1)	// Blue
                   , vec4( 255, 255,   0, 1)	// Yellow
                   , vec4( 255,   0, 255, 1)	// Fuchsia
                   , vec4(   0, 255, 255, 1)	// Cyan, Aqua
				   , vec4( 255, 165,   0, 1)	// Orange
				   , vec4( 238, 130, 238, 1)	// Violet
				   , vec4( 165,  42,  42, 1)	// Brown
				   , vec4( 189, 183, 107, 1)	// DarkKhaki
				   , vec4( 128,   9, 128, 1)	// Purple
				   , vec4( 255, 215,   0, 1)	// Gold
				   , vec4( 255, 192, 203, 1)	// Pink
				   , vec4( 128, 128,   0, 1)	// Olive
				   , vec4( 154, 205,  50, 1)	// YellowGreen
				   , vec4(   0, 128,   0, 1)	// Lime
				   , vec4( 255,  20, 147, 1)	// DeepPink
                   };
const vec4 div = vec4(255,255,255,1);
const vec4 colors[] = { (rgb[ 0]/div)
                      , (rgb[ 1]/div)
                      , (rgb[ 2]/div)
                      , (rgb[ 3]/div)
                      , (rgb[ 4]/div)
                      , (rgb[ 5]/div)
                      , (rgb[ 6]/div)
                      , (rgb[ 7]/div)
                      , (rgb[ 8]/div)
                      , (rgb[ 9]/div)
                      , (rgb[10]/div)
                      , (rgb[11]/div)
                      , (rgb[12]/div)
                      , (rgb[13]/div)
                      , (rgb[14]/div)
                      , (rgb[15]/div)
                      };
void setBit(in out uvec4 ballot, uint bit) {
	uint t[5];
	t[0] = ballot.x; t[1] = ballot.y; t[2] = ballot.z; t[3] = ballot.w;
	t[min(128, bit) / 32] |= (1u << (bit % 32));
	ballot.x = t[0]; ballot.y = t[1]; ballot.z = t[2]; ballot.w = t[3];
}
void resetBit(in out uvec4 ballot, uint bit) {
	uint t[5];
	t[0] = ballot.x; t[1] = ballot.y; t[2] = ballot.z; t[3] = ballot.w;
	t[min(128, bit) / 32] &= (UINT_MAX ^ (1u << (bit % 32)));
	ballot.x = t[0]; ballot.y = t[1]; ballot.z = t[2]; ballot.w = t[3];
}
uint fragmentIndex() { return (uint(gl_FragCoord.y) * width + uint(gl_FragCoord.x)); }
uint findNonHelperElectInvocation()
{
	uvec4	nonHelperBallot = uvec4(0);
	if ( ! helperInvocationEXT() )
	{
		nonHelperBallot = subgroupBallot(true);
	}
	nonHelperBallot = subgroupOr(nonHelperBallot);
	const uint lsb = subgroupBallotFindLSB(nonHelperBallot);
	return min(lsb, gl_SubgroupSize);
}
bool subgroupNonHelperElect()
{
	return (gl_SubgroupInvocationID == findNonHelperElectInvocation());
}
bool baseIsRootX(const uint base, const uint right, const uint btm, const uint diag)
{
	uint k = fragmentIndex();
	uint l = subgroupShuffle(k, (gl_SubgroupInvocationID + 1));
	uint m = subgroupShuffle(k, (gl_SubgroupInvocationID + 2));
	uint n = subgroupShuffle(k, (gl_SubgroupInvocationID + 3));

	bool i0 = ((k + 1) / (base  + 1)) == 1 && ((k + 1) % (base  + 1)) == 0;
	bool i1 = ((l + 1) / (right + 1)) == 1 && ((l + 1) % (right + 1)) == 0;
	bool i2 = ((m + 1) / (btm   + 1)) == 1 && ((m + 1) % (btm   + 1)) == 0;
	bool i3 = ((n + 1) / (diag  + 1)) == 1 && ((n + 1) % (diag  + 1)) == 0;

	return i0 && i1 && i2 && i3;
}
bool isQuadLeader(const uint fragmentID)
{
	const uint baseInvocationID		= outputP.p[fragmentID               * primitiveStride + gl_PrimitiveID] & 0xFFFF;
	const uint rightInvocationID	= outputP.p[(fragmentID         + 1) * primitiveStride + gl_PrimitiveID] & 0xFFFF;
	const uint bottomInvocationID	= outputP.p[(fragmentID + width    ) * primitiveStride + gl_PrimitiveID] & 0xFFFF;
	const uint diagonalInvocationID	= outputP.p[(fragmentID + width + 1) * primitiveStride + gl_PrimitiveID] & 0xFFFF;

	return ((baseInvocationID % 4) == 0)
		&& ((rightInvocationID - 1) == baseInvocationID)
		&& ((bottomInvocationID - 1) == rightInvocationID)
		&& ((diagonalInvocationID - 1) == bottomInvocationID);
}
void mapFragmentsToInvocationsAndDerivatives(const uint subgroupID)
{
	const uint localPrimitiveID		= gl_PrimitiveID;
	const uint localFragmentID		= fragmentIndex();
	const uint localInvocationIndex	= subgroupID * gl_SubgroupSize + gl_SubgroupInvocationID;
	const float derivativeHorz		= dFdx(localFragmentID);
	const float derivativeVert		= dFdy(localFragmentID);

	uint swapHorz	= localFragmentID;
	uint swapDiag	= localFragmentID;
	uint swapVert	= localFragmentID;
	swapHorz		= subgroupQuadSwapHorizontal(swapHorz);
	swapDiag		= subgroupQuadSwapDiagonal(swapDiag);
	swapVert		= subgroupQuadSwapVertical(swapVert);

	uint broadcastValue		= localPrimitiveID;
	uint broadcastResult	= subgroupQuadBroadcast(broadcastValue, 0);

	uint clusteredMin	= subgroupClusteredMin(localPrimitiveID, 4);
	uint clusteredMax	= subgroupClusteredMax(localPrimitiveID, 4);

	uvec4 helperInvocationBits	= uvec4(0);

	// Map non-helper invocations block
	if ( ! helperInvocationEXT() )
	{
		uint idx = localFragmentID * primitiveStride + localPrimitiveID;
		outputP.p[idx] = ((subgroupID + 1) << 16) | gl_SubgroupInvocationID;
		outputDerivatives.d[idx * drvSize + 0]	= idx + 1;
		outputDerivatives.d[idx * drvSize + 1]	= localFragmentID;
		outputDerivatives.d[idx * drvSize + 2]	= uint(derivativeHorz);
		outputDerivatives.d[idx * drvSize + 3]	= uint(derivativeVert);
		outputDerivatives.d[idx * drvSize + 4]	= swapHorz;
		outputDerivatives.d[idx * drvSize + 5]	= swapDiag;
		outputDerivatives.d[idx * drvSize + 6]	= swapVert;
		outputDerivatives.d[idx * drvSize + 7]	= broadcastResult;
		outputDerivatives.d[idx * drvSize + 8]	= broadcastValue;
		outputDerivatives.d[idx * drvSize + 9]	= clusteredMin;
		outputDerivatives.d[idx * drvSize + 10]	= clusteredMax;
		outputDerivatives.d[idx * drvSize + 11]	= 0x07070707;
	}
	else
	{
		helperInvocationBits = subgroupBallot(true);
	}
	helperInvocationBits = subgroupOr(helperInvocationBits);

	// Map helper invocations block
	{
		uvec4 tmpHelperBits				= helperInvocationBits;
		uint helperSubgroupInvocationID	= subgroupBallotFindLSB(tmpHelperBits);
		while (subgroupBallotBitExtract(tmpHelperBits, helperSubgroupInvocationID))
		{
			uint  helperInvocationIndex	= subgroupShuffle(localInvocationIndex, helperSubgroupInvocationID);
			uint  helperSubgroupID		= subgroupShuffle(subgroupID, helperSubgroupInvocationID);
			uint  helperFragmentID		= subgroupShuffle(localFragmentID, helperSubgroupInvocationID);
			uint  helperPrimitiveID		= subgroupShuffle(localPrimitiveID, helperSubgroupInvocationID);

			float helperDerivativeHorz	= subgroupShuffle(derivativeHorz, helperSubgroupInvocationID);
			float helperDerivativeVert	= subgroupShuffle(derivativeVert, helperSubgroupInvocationID);

			uint  helperSwapHorz		= subgroupShuffle(swapHorz, helperSubgroupInvocationID);
			uint  helperSwapDiag		= subgroupShuffle(swapVert, helperSubgroupInvocationID);
			uint  helperSwapVert		= subgroupShuffle(swapVert, helperSubgroupInvocationID);

			uint  helperBroadcastResult	= subgroupShuffle(broadcastResult, helperSubgroupInvocationID);
			uint  helperBroadcastValue	= subgroupShuffle(broadcastValue, helperSubgroupInvocationID);

			uint  helperClusteredMin	= subgroupShuffle(clusteredMin, helperSubgroupInvocationID);
			uint  helperClusteredMax	= subgroupShuffle(clusteredMax, helperSubgroupInvocationID);

			if (subgroupNonHelperElect())
			{
				uint idx = helperFragmentID * primitiveStride + helperPrimitiveID;
				outputP.p[idx] = (((helperSubgroupID + 1) | 0x8000) << 16) | helperSubgroupInvocationID;
				outputDerivatives.d[idx * drvSize + 0]	= idx + 1;
				outputDerivatives.d[idx * drvSize + 1]	= helperFragmentID;
				outputDerivatives.d[idx * drvSize + 2]	= uint(helperDerivativeHorz);
				outputDerivatives.d[idx * drvSize + 3]	= uint(helperDerivativeVert);
				outputDerivatives.d[idx * drvSize + 4]	= helperSwapHorz;
				outputDerivatives.d[idx * drvSize + 5]	= helperSwapDiag;
				outputDerivatives.d[idx * drvSize + 6]	= helperSwapVert;
				outputDerivatives.d[idx * drvSize + 7]	= helperBroadcastResult;
				outputDerivatives.d[idx * drvSize + 8]	= helperBroadcastValue;
				outputDerivatives.d[idx * drvSize + 9]	= helperClusteredMin;
				outputDerivatives.d[idx * drvSize + 10]	= helperClusteredMax;
				outputDerivatives.d[idx * drvSize + 11] = 0x13131313;
			}
			resetBit(tmpHelperBits, helperSubgroupInvocationID);
			helperSubgroupInvocationID = subgroupBallotFindLSB(tmpHelperBits);
		}
	}
}

void main()
{
	uint tmpSubgroupID              = 0u;
	uint helperInvocationCount		= 0u;
	uint nonHelperInvocationCount	= 0u;
	uvec4 helperInvocationsBits		= uvec4(0, 0, 0, 0);
	uvec4 nonHelperInvocationsBits	= uvec4(0, 0, 0, 0);
	if (gl_HelperInvocation)
	{
		helperInvocationsBits = subgroupBallot(true);
		helperInvocationCount = 1u;
	}
	else
	{
		nonHelperInvocationsBits = subgroupBallot(true);
		nonHelperInvocationCount = 1u;
	}
	helperInvocationsBits			= subgroupOr(helperInvocationsBits);
	nonHelperInvocationsBits		= subgroupOr(nonHelperInvocationsBits);
	uint helperBitCount				= subgroupBallotBitCount(helperInvocationsBits);
	uint nonHelperBitCount			= subgroupBallotBitCount(nonHelperInvocationsBits);
	helperInvocationCount			= subgroupAdd(helperInvocationCount);
	nonHelperInvocationCount		= subgroupAdd(nonHelperInvocationCount);
	const uint nonHelperElectBit	= subgroupBallotFindLSB(nonHelperInvocationsBits);
	if (gl_SubgroupInvocationID == nonHelperElectBit)
	{
		atomicAdd(outputP.p[(width * height + NUM_SUBGROUPS) * primitiveStride + gl_PrimitiveID], 1);
		tmpSubgroupID = atomicAdd(outputP.p[(width * height + UNIQUE_NUM_SUBGROUPS) * primitiveStride], 1);
		outputP.p[(width * height + SUBGROUP_SIZE) * primitiveStride + gl_PrimitiveID] = gl_SubgroupSize;
		atomicAdd(outputP.p[(width * height + NON_HELPER_COUNT) * primitiveStride + gl_PrimitiveID], nonHelperInvocationCount);
		atomicAdd(outputP.p[(width * height + HELPER_COUNT) * primitiveStride + gl_PrimitiveID], helperInvocationCount);
	}
	const uint fragmentID = fragmentIndex();
	const uint subgroupID = subgroupShuffle(tmpSubgroupID, nonHelperElectBit);
	const uint numSubgroups = outputP.p[(width * height + NUM_SUBGROUPS) * primitiveStride + gl_PrimitiveID];

	mapFragmentsToInvocationsAndDerivatives(subgroupID);

	uint colorSubgroupID_0 = ((outputP.p[fragmentID * primitiveStride + 0] >> 16) & 0x7FFF);
	uint colorSubgroupID_1 = colorSubgroupID_0;
	vec4 subgroupColor = colors[subgroupID % colors.length()];
	if (colorSubgroupID_0 > 0)
	{
		subgroupColor = colors[(colorSubgroupID_0 - 1) % colors.length()];
		if (primitiveStride > 1)
		{
			colorSubgroupID_1 = ((outputP.p[fragmentID * primitiveStride + 1] >> 16) & 0x7FFF);
			if (colorSubgroupID_1 > 0)
			{
				subgroupColor = mix(colors[(colorSubgroupID_1 - 1) % colors.length()], subgroupColor, 0.5);
			}
		}
	}
	else if (primitiveStride > 1)
	{
		colorSubgroupID_1 = ((outputP.p[fragmentID * primitiveStride + 1] >> 16) & 0x7FFF);
		if (colorSubgroupID_1 > 0)
		{
			subgroupColor = colors[(colorSubgroupID_1 - 1) % colors.length()];
		}
	}
	const vec4 color = ((USE_COLOR_GROUP & flags) != 0) ? subgroupColor : colors[gl_PrimitiveID % colors.length()];

	const uint rightFragmentID		= inputQuads.qp[0].right.y * width + inputQuads.qp[0].right.x;
	const uint rightSubgroupInfo	= outputP.p[rightFragmentID * primitiveStride + gl_PrimitiveID];
	const uint rightInvocationID	= rightSubgroupInfo & 0xFFFF;
	const uint rightSubgroupID		= ((rightSubgroupInfo >> 16) & 0x7FFF) - 1;

	PixelInfo right;
	{
		initStructure(right);
		const uint drvOffset = (rightFragmentID * primitiveStride + gl_PrimitiveID) * drvSize;

		right.coordX			= inputQuads.qp[0].right.x;
		right.coordY			= inputQuads.qp[0].right.y;
		right.subgroupInfo		= rightSubgroupInfo;
		right.entryIndex		= outputDerivatives.d[drvOffset + 0];
		right.fragmentID		= outputDerivatives.d[drvOffset + 1];
		right.derivativeX		= outputDerivatives.d[drvOffset + 2];
		right.derivativeY		= outputDerivatives.d[drvOffset + 3];
		right.swapHorz			= outputDerivatives.d[drvOffset + 4];
		right.swapDiag			= outputDerivatives.d[drvOffset + 5];
		right.swapVert			= outputDerivatives.d[drvOffset + 6];
		right.broadcastResult	= outputDerivatives.d[drvOffset + 7];
		right.broadcastValue	= outputDerivatives.d[drvOffset + 8];
		right.clusteredMin		= outputDerivatives.d[drvOffset + 9];
		right.clusteredMax		= outputDerivatives.d[drvOffset + 10];
		right.quadLeader		= isQuadLeader(rightFragmentID) ? 1 : 0;
		right.data1 = 0;
		right.data2 = 0;
		right.data3 = 0;
		right.data4 = 0;
		right.data5 = 0;   
		right.data6 = 0;
		right.data7 = 0;
		right.data8 = 0;
	}

	const uint diagonalFragmentID	= inputQuads.qp[0].diagonal.y * width + inputQuads.qp[0].diagonal.x;
	const uint diagonalSubgroupInfo	= outputP.p[diagonalFragmentID * primitiveStride + gl_PrimitiveID];
	const uint diagonalInvocationID	= diagonalSubgroupInfo & 0xFFFF;
	const uint diagonalSubgroupID	= ((diagonalSubgroupInfo >> 16) & 0x7FFF) - 1;

	PixelInfo diagonal;
	{
		initStructure(diagonal);
		const uint drvOffset = (diagonalFragmentID * primitiveStride + gl_PrimitiveID) * drvSize;

		diagonal.coordX				= inputQuads.qp[0].diagonal.x;
		diagonal.coordY				= inputQuads.qp[0].diagonal.y;
		diagonal.subgroupInfo		= diagonalSubgroupInfo;
		diagonal.entryIndex			= outputDerivatives.d[drvOffset + 0];
		diagonal.fragmentID			= outputDerivatives.d[drvOffset + 1];
		diagonal.derivativeX		= outputDerivatives.d[drvOffset + 2];
		diagonal.derivativeY		= outputDerivatives.d[drvOffset + 3];
		diagonal.swapHorz			= outputDerivatives.d[drvOffset + 4];
		diagonal.swapDiag			= outputDerivatives.d[drvOffset + 5];
		diagonal.swapVert			= outputDerivatives.d[drvOffset + 6];
		diagonal.broadcastResult	= outputDerivatives.d[drvOffset + 7];
		diagonal.broadcastValue		= outputDerivatives.d[drvOffset + 8];
		diagonal.clusteredMin		= outputDerivatives.d[drvOffset + 9];
		diagonal.clusteredMax		= outputDerivatives.d[drvOffset + 10];
		diagonal.quadLeader			= isQuadLeader(diagonalFragmentID) ? 1 : 0;
	}

	const uint bottomFragmentID		= inputQuads.qp[0].bottom.y * width + inputQuads.qp[0].bottom.x;
	const uint bottomSubgroupInfo	= outputP.p[bottomFragmentID * primitiveStride + gl_PrimitiveID];
	const uint bottomInvocationID	= bottomSubgroupInfo & 0xFFFF;
	const uint bottomSubgroupID		= ((bottomSubgroupInfo >> 16) & 0x7FFF) - 1;

	PixelInfo bottom;
	{
		initStructure(bottom);
		const uint drvOffset = (bottomFragmentID * primitiveStride + gl_PrimitiveID) * drvSize;

		bottom.coordX			= inputQuads.qp[0].bottom.x;
		bottom.coordY			= inputQuads.qp[0].bottom.y;
		bottom.subgroupInfo		= bottomSubgroupInfo;
		bottom.entryIndex		= outputDerivatives.d[drvOffset + 0];
		bottom.fragmentID		= outputDerivatives.d[drvOffset + 1];
		bottom.derivativeX		= outputDerivatives.d[drvOffset + 2];
		bottom.derivativeY		= outputDerivatives.d[drvOffset + 3];
		bottom.swapHorz			= outputDerivatives.d[drvOffset + 4];
		bottom.swapDiag			= outputDerivatives.d[drvOffset + 5];
		bottom.swapVert			= outputDerivatives.d[drvOffset + 6];
		bottom.broadcastResult	= outputDerivatives.d[drvOffset + 7];
		bottom.broadcastValue	= outputDerivatives.d[drvOffset + 8];
		bottom.clusteredMin		= outputDerivatives.d[drvOffset + 9];
		bottom.clusteredMax		= outputDerivatives.d[drvOffset + 10]; 
		bottom.quadLeader		= isQuadLeader(bottomFragmentID) ? 1 : 0;
	}

	const uint baseFragmentID		= inputQuads.qp[0].base.y * width + inputQuads.qp[0].base.x;
	const uint baseSubgroupInfo		= outputP.p[baseFragmentID * primitiveStride + gl_PrimitiveID];
	const uint baseInvocationID		= baseSubgroupInfo & 0xFFFF;
	const uint baseSubgroupID		= ((baseSubgroupInfo >> 16) & 0x7FFF) - 1;

	PixelInfo base;
	{
		initStructure(base);
		const uint drvOffset = (baseFragmentID * primitiveStride + gl_PrimitiveID) * drvSize;

		base.coordX				= inputQuads.qp[0].base.x;
		base.coordY				= inputQuads.qp[0].base.y;
		base.subgroupInfo		= baseSubgroupInfo;
		base.entryIndex			= outputDerivatives.d[drvOffset + 0];
		base.fragmentID			= outputDerivatives.d[drvOffset + 1];
		base.derivativeX		= outputDerivatives.d[drvOffset + 2];
		base.derivativeY		= outputDerivatives.d[drvOffset + 3];
		base.swapHorz			= outputDerivatives.d[drvOffset + 4];
		base.swapDiag			= outputDerivatives.d[drvOffset + 5];
		base.swapVert			= outputDerivatives.d[drvOffset + 6];
		base.broadcastResult	= outputDerivatives.d[drvOffset + 7];
		base.broadcastValue		= outputDerivatives.d[drvOffset + 8];
		base.clusteredMin		= outputDerivatives.d[drvOffset + 9];
		base.clusteredMax		= outputDerivatives.d[drvOffset + 10];
		base.quadLeader			= isQuadLeader(baseFragmentID) ? 1 : 0;
	}

	if (subgroupID == baseSubgroupID && subgroupNonHelperElect())
	{
		outputQuads.qi[gl_PrimitiveID].right	= right;
		outputQuads.qi[gl_PrimitiveID].diagonal	= diagonal;
		outputQuads.qi[gl_PrimitiveID].bottom	= bottom;
		outputQuads.qi[gl_PrimitiveID].base		= base;
	}

	vec4 selectedColor = vec4(1,0,0,1);

	if (((PERFORM_DEMOTING_BIT & flags) != 0) && baseSubgroupID == subgroupID && gl_SubgroupInvocationID == baseInvocationID)
	{
		selectedColor = uvec4(1,0,1,1);
		demote;
	}
	if (((PERFORM_DEMOTING_BIT & flags) != 0) && ((DEMOTE_WHOLE_QUAD_BIT & flags) != 0)
		&& rightSubgroupID == subgroupID && gl_SubgroupInvocationID == rightInvocationID)
	{
		selectedColor = uvec4(1,0,1,1);
		demote;
	}
	if (((PERFORM_DEMOTING_BIT & flags) != 0) && ((DEMOTE_WHOLE_QUAD_BIT & flags) != 0)
		&& bottomSubgroupID == subgroupID && gl_SubgroupInvocationID == bottomInvocationID)
	{
		selectedColor = uvec4(1,0,1,1);
		demote;
	}
	if (((PERFORM_DEMOTING_BIT & flags) != 0) && ((DEMOTE_WHOLE_QUAD_BIT & flags) != 0)
		&& diagonalSubgroupID == subgroupID && gl_SubgroupInvocationID == diagonalInvocationID)
	{
		selectedColor = uvec4(1,0,1,1);
		demote;
	}

	mapFragmentsToInvocationsAndDerivatives(subgroupID);

	if (uint(gl_FragCoord.x) == base.coordX && uint(gl_FragCoord.y) == base.coordY)
		attachment = selectedColor;
	else attachment = color;
}
