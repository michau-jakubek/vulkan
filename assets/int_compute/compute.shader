
#extension GL_EXT_subgroup_uniform_control_flow : require
#extension GL_KHR_shader_subgroup_basic : require
#extension GL_KHR_shader_subgroup_ballot : require
#extension GL_KHR_shader_subgroup_arithmetic : require
#extension GL_KHR_shader_subgroup_shuffle : require
#extension GL_EXT_debug_printf : require


layout(local_size_x_id = 2147483644,
       local_size_y_id = 2147483645,
	   local_size_z_id = 2147483646) in;

layout(push_constant) uniform PushConstant
{
	uint addressingMode;
	uint checkpointMax;
	uint controlIndex0;
	uint controlIndex1;
	uint controlIndex2;
	uint controlIndex3;
	uint controlIndex4;
};
layout(binding = 0) buffer OutputParams { uint p[]; } outputP;
layout(binding = 1) buffer InvocationsTinput { uint t[]; } inputT;
layout(binding = 2) buffer InvocationsIinput { uint i[]; } inputI;
layout(binding = 3) buffer InvocationsJinput { uint j[]; } inputJ;
layout(binding = 4) buffer InvocationsRoutput { uint r[]; } outputR;

#define UINT_MAX 0xFFFFFFFF
#define TYPE_Uint32		0
#define TYPE_Int32		1
#define TYPE_Float32	2

uint loc() { return gl_LocalInvocationIndex; }

void writeParams()
{
	if (gl_LocalInvocationIndex == 0)
	{
		outputP.p[0] = gl_NumWorkGroups.x;
		outputP.p[1] = gl_NumWorkGroups.y;
		outputP.p[2] = gl_NumWorkGroups.z;
		outputP.p[3] = gl_WorkGroupSize.x;
		outputP.p[4] = gl_WorkGroupSize.y;
		outputP.p[5] = gl_WorkGroupSize.z;
		// outputP.p[6]
		outputP.p[7] = gl_NumSubgroups;
		outputP.p[8] = gl_SubgroupSize;
		outputP.p[9] = addressingMode;
		outputP.p[10] = checkpointMax;
		outputP.p[11] = inputI.i.length();	// ROUNDUP( (local_size_x * local_size_y * local_size_z), gl_SubgroupSize )
		outputP.p[12] = outputP.p.length();
		//---------------------------------
		outputP.p[16] = (controlIndex0 < inputI.i.length()) ? inputI.i[controlIndex0] : UINT_MAX;
		outputP.p[17] = (controlIndex1 < inputI.i.length()) ? inputI.i[controlIndex1] : UINT_MAX;
		outputP.p[18] = (controlIndex2 < inputI.i.length()) ? inputI.i[controlIndex2] : UINT_MAX;
		outputP.p[19] = (controlIndex3 < inputI.i.length()) ? inputI.i[controlIndex3] : UINT_MAX;
		outputP.p[20] = (controlIndex4 < inputI.i.length()) ? inputI.i[controlIndex4] : UINT_MAX;
		//---------------------------------
		outputP.p[32] = (controlIndex0 < inputJ.j.length()) ? inputJ.j[controlIndex0] : UINT_MAX;
		outputP.p[33] = (controlIndex1 < inputJ.j.length()) ? inputJ.j[controlIndex1] : UINT_MAX;
		outputP.p[34] = (controlIndex2 < inputJ.j.length()) ? inputJ.j[controlIndex2] : UINT_MAX;
		outputP.p[35] = (controlIndex3 < inputJ.j.length()) ? inputJ.j[controlIndex3] : UINT_MAX;
		outputP.p[36] = (controlIndex4 < inputJ.j.length()) ? inputJ.j[controlIndex4] : UINT_MAX;
	}
	uint iMax = gl_WorkGroupSize.x * gl_WorkGroupSize.y * gl_WorkGroupSize.z - 1;
	if (gl_LocalInvocationIndex == iMax)
	{
		outputP.p[6] = iMax;
	}
}

float loadFloatI(uint at) {
	return ((inputT.t[at] & 0xFFFF) == TYPE_Float32)
		? uintBitsToFloat(inputI.i[at])
		: ((inputT.t[at] & 0xFFFF) == TYPE_Int32)
			? float(int(inputI.i[at]))
			: float(inputI.i[at]);
}

int loadIntI(uint at) {
	return ((inputT.t[at] & 0xFFFF) == TYPE_Float32)
		? int(uintBitsToFloat(inputI.i[at]))
		: int(inputI.i[at]);
}

uint loadUintI(uint at) {
	return ((inputT.t[at] & 0xFFFF) == TYPE_Float32)
		? uint(uintBitsToFloat(inputI.i[at]))
		: inputI.i[at];
}

float loadFloatJ(uint at) {
	return ((inputT.t[at] >> 16) == TYPE_Float32)
		? uintBitsToFloat(inputJ.j[at])
		: ((inputT.t[at] >> 16) == TYPE_Int32)
			? float(int(inputJ.j[at]))
			: float(inputJ.j[at]);
}

int loadIntJ(uint at) {
	return ((inputT.t[at] >> 16) == TYPE_Float32)
		? int(uintBitsToFloat(inputJ.j[at]))
		: int(inputJ.j[at]);
}

uint loadUintJ(uint at) {
	return ((inputT.t[at] >> 16) == TYPE_Float32)
		? uint(uintBitsToFloat(inputJ.j[at]))
		: inputJ.j[at];
}

void forceStoreFloat(float x, uint at) {
	if (at >= inputT.t.length()) return;
	inputT.t[at] = ((inputT.t[at] >> 16) << 16) | TYPE_Float32;
	outputR.r[at] = floatBitsToUint(x);
}

void forceStoreInt(int x, uint at) {
	if (at >= inputT.t.length()) return;
	inputT.t[at] = ((inputT.t[at] >> 16) << 16) | TYPE_Int32;
	outputR.r[at] = uint(x);
}

void forceStoreUint(uint x, uint at) {
	if (at >= inputT.t.length()) return;
	inputT.t[at] = ((inputT.t[at] >> 16) << 16) | TYPE_Uint32;
	outputR.r[at] = x;
}

void storeFloat(float x, uint at) {
	if (at >= inputT.t.length()) return;
	outputR.r[at] = ((inputT.t[at] & 0xFFFF) == TYPE_Float32)
						? floatBitsToUint(x)
						: ((inputT.t[at] & 0xFFFF) == TYPE_Int32)
							? uint(int(x))
							: uint(x);
}

void storeUint(uint x, uint at) {
	if (at >= inputT.t.length()) return;
	outputR.r[at] = ((inputT.t[at] & 0xFFFF) == TYPE_Float32)
						? floatBitsToUint(float(x))
						: ((inputT.t[at] & 0xFFFF) == TYPE_Int32)
							? uint(int(x))
							: x;
}

void storeInt(int x, uint at) {
	if (at >= inputT.t.length()) return;
	outputR.r[at] = ((inputT.t[at] & 0xFFFF) == TYPE_Float32)
						? floatBitsToUint(float(x))
						: uint(x);
}

void xmain();

void main_entry()
{
	writeParams();
	xmain();
}















